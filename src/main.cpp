#include <QtWidgets>
#include <algorithm>
#include <QtNetwork>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QXmlStreamReader>
#include <QUrlQuery>
struct Airport { QString name, path, icao, source; };
static Airport identifyPackage(const QString &path){
 Airport a; a.path=path; a.name=QFileInfo(path).fileName();
 QRegularExpression explicitCode("(?:^|[-_ .])(?:airport|apt|icao)[-_ .]+([A-Z]{4})(?:$|[-_ .])",QRegularExpression::CaseInsensitiveOption);
 QRegularExpression codeToken("(?:^|[-_ .])([A-Z]{4})(?:$|[-_ .])",QRegularExpression::CaseInsensitiveOption);
 QSet<QString> candidates;
 auto extract=[&](const QString &value,bool explicitOnly)->QString{
  auto m=(explicitOnly?explicitCode:codeToken).match(value);
  if(!m.hasMatch())return {};
  QString c=m.captured(1).toUpper();
  static const QSet<QString> invalid={"MSFS","ASOBO","BASE","DATA","SCEN","PACK","CITY","TEXT","BGLS","FILE","MODE"};
  return invalid.contains(c)?QString():c;
 };
 // layout.json lists BGL files without opening large scenery binaries.
 QFile layout(path+"/layout.json");
 if(layout.open(QIODevice::ReadOnly)){
  auto doc=QJsonDocument::fromJson(layout.readAll());
  for(const auto &v:doc.array()){
   QString p=v.toObject().value("path").toString();
   if(!p.endsWith(".bgl",Qt::CaseInsensitive))continue;
   QString c=extract(QFileInfo(p).completeBaseName(),true);
   if(!c.isEmpty())candidates.insert(c);
  }
 }
 if(candidates.size()>1){a.source="Plusieurs ICAO possibles (layout)";return a;}
 if(candidates.size()==1){a.icao=*candidates.cbegin();a.source="Layout BGL (indice)";return a;}
 // Bounded fallback for packages without a complete layout.
 QDir scenery(path+"/scenery");
 if(scenery.exists()){
  QStringList dirs={scenery.absolutePath()};int visited=0;
  while(!dirs.isEmpty()&&visited++<120){
   QDir d(dirs.takeLast());
   for(const auto &fi:d.entryInfoList(QDir::Files|QDir::NoDotAndDotDot)){
    if(fi.suffix().compare("bgl",Qt::CaseInsensitive)==0){
     QString c=extract(fi.completeBaseName(),true);
     if(!c.isEmpty())candidates.insert(c);
    }
   }
   for(const auto &di:d.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks))dirs<<di.absoluteFilePath();
  }
 }
 if(candidates.size()>1){a.source="Plusieurs ICAO possibles (BGL)";return a;}
 if(candidates.size()==1){a.icao=*candidates.cbegin();a.source="BGL (indice)";return a;}
 QFile manifest(path+"/manifest.json");
 if(manifest.open(QIODevice::ReadOnly)){
  auto doc=QJsonDocument::fromJson(manifest.read(65536));
  QString c=extract(doc.object().value("title").toString(),true);
  if(!c.isEmpty()){a.icao=c;a.source="Manifest (indice)";return a;}
 }
 QString c=extract(a.name,true);
 if(!c.isEmpty()){a.icao=c;a.source="Dossier (indice)";return a;}
 c=extract(a.name,false);
 if(!c.isEmpty()){a.icao=c;a.source="Dossier (supposé)";}
 return a;
}
class Window : public QWidget {
 Q_OBJECT
 QLineEdit *user,*from,*to; QTableWidget *table,*matches; QLabel *info; QNetworkAccessManager net; QVector<Airport> airports; QStringList roots; QString alt; QFutureWatcher<QVector<Airport>> watcher; QPushButton *refreshButton=nullptr; QProgressBar *progress=nullptr; QLabel *scanDetail=nullptr,*libraryTitle=nullptr,*durationLabel=nullptr; QLineEdit *alternate=nullptr; QSet<QString> selectedPaths; bool filling=false; QHash<QString,QString> overrides; QComboBox *fontChoice=nullptr;
 static QString code(const QString &s) { QRegularExpression re("(?<![A-Z0-9])([A-Z]{4})(?![A-Z0-9])"); auto m=re.match(s.toUpper());return m.hasMatch()?m.captured(1):QString(); }
 static QString xmlValue(const QByteArray &bytes,const QString &parent,const QString &child) {QXmlStreamReader x(bytes);QString current;while(!x.atEnd()){x.readNext();if(x.isStartElement()){QString tag=x.name().toString();if(tag==parent)current=parent;else if(current==parent&&tag==child)return x.readElementText().trimmed();}else if(x.isEndElement()&&x.name()==parent)current.clear();}return {}; }
 static void addRow(QTableWidget *t,const QStringList &values) {int r=t->rowCount();t->insertRow(r);for(int i=0;i<values.size();++i)t->setItem(r,i,new QTableWidgetItem(values[i]));}
 void choose(const QString &path,bool checked){
  auto it=std::find_if(airports.cbegin(),airports.cend(),[&](const Airport &a){return a.path==path;});
  if(it==airports.cend())return;
  if(checked && (it->icao.isEmpty()||it->source!="Validé manuellement")){QMessageBox::information(this,"ICAO non confirmé","Le code ICAO doit être validé manuellement avant sélection. Double-clique sur sa cellule ICAO dans la bibliothèque.");render();return;}
  if(checked){for(const auto &a:airports)if(a.icao==it->icao)selectedPaths.remove(a.path);selectedPaths.insert(path);}
  else selectedPaths.remove(path);
  QSettings("FlightSceneryManager","FlightSceneryManager").setValue("selectedPaths",QStringList(selectedPaths.values()));
  render();
 }
 void suggest(){
  filling=true;matches->setRowCount(0);
  QStringList codes={from->text().trimmed().toUpper(),to->text().trimmed().toUpper(),alternate->text().trimmed().toUpper()};
  QSet<QString> used;
  for(const auto &icao:codes){
   if(icao.size()!=4||used.contains(icao))continue;used.insert(icao);
   bool found=false;
   for(const auto &a:airports){
    if(a.icao!=icao)continue;found=true;
    int row=matches->rowCount();matches->insertRow(row);
    auto *check=new QTableWidgetItem;check->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);check->setCheckState(selectedPaths.contains(a.path)?Qt::Checked:Qt::Unchecked);check->setData(Qt::UserRole,a.path);matches->setItem(row,0,check);
    matches->setItem(row,1,new QTableWidgetItem(icao));
    matches->setItem(row,2,new QTableWidgetItem(a.name));
    matches->setItem(row,3,new QTableWidgetItem(selectedPaths.contains(a.path)?"Sélectionnée":a.source.isEmpty()?"ICAO inconnu":a.source));
   }
   if(!found){int row=matches->rowCount();matches->insertRow(row);matches->setItem(row,1,new QTableWidgetItem(icao));matches->setItem(row,2,new QTableWidgetItem("Non trouvée"));matches->setItem(row,3,new QTableWidgetItem("À vérifier"));}
  }
  filling=false;
 }
 void validateSelections(){QSet<QString> seen;QSet<QString> safe;for(const auto &a:airports){if(!selectedPaths.contains(a.path)||a.icao.isEmpty()||a.source!="Validé manuellement"||seen.contains(a.icao))continue;seen.insert(a.icao);safe.insert(a.path);}selectedPaths=safe;QSettings("FlightSceneryManager","FlightSceneryManager").setValue("selectedPaths",QStringList(selectedPaths.values()));}
 void render(){
  filling=true;table->setUpdatesEnabled(false);table->setRowCount(airports.size());
  QSet<QString> codes;int identified=0;
  for(int i=0;i<airports.size();++i){
   const auto &a=airports[i];if(!a.icao.isEmpty()){codes.insert(a.icao);++identified;}
   auto *icaoItem=new QTableWidgetItem(a.icao.isEmpty()?"?":a.icao);icaoItem->setToolTip(a.source.isEmpty()?"ICAO non identifié":a.source);table->setItem(i,0,icaoItem);
   auto *check=new QTableWidgetItem;check->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);check->setCheckState(selectedPaths.contains(a.path)?Qt::Checked:Qt::Unchecked);check->setData(Qt::UserRole,a.path);table->setItem(i,1,check);
   table->setItem(i,2,new QTableWidgetItem(a.name));table->setItem(i,3,new QTableWidgetItem(a.path));
  }
  table->setUpdatesEnabled(true);
  libraryTitle->setText(QString("BIBLIOTHÈQUE DE SCÈNES — %1 ICAO distincts / %2 packages (%3 ICAO inconnus) • Double-clic ICAO pour valider").arg(codes.size()).arg(airports.size()).arg(airports.size()-identified));
  filling=false;suggest();
 }
 QString inventoryPath() const {return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/inventory.json";}
 void loadCache(){QFile file(inventoryPath());if(!file.open(QIODevice::ReadOnly))return;auto doc=QJsonDocument::fromJson(file.readAll());auto arr=doc.object().value("packages").toArray();for(const auto &v:arr){auto o=v.toObject();airports.push_back({o.value("name").toString(),o.value("path").toString(),o.value("icao_candidate").toString(),o.value("icao_source").toString()});}for(auto &a:airports)if(overrides.contains(a.path)){a.icao=overrides.value(a.path);a.source="Validé manuellement";}validateSelections();render();info->setText(QString("%1 packages en cache • Actualiser pour rescanner").arg(airports.size()));}
 void scan(){
 if(watcher.isRunning())return;
 refreshButton->setEnabled(false);progress->show();progress->setRange(0,0);
 scanDetail->setText("Préparation de l'analyse...");
 info->setText("Recherche optimisée en arrière-plan...");
 QStringList paths=roots;
 QPointer<Window> self(this);
 watcher.setFuture(QtConcurrent::run([paths,self](){
  QVector<Airport> result;QSet<QString> seen;
  QStringList pending=paths;int checked=0;
  while(!pending.isEmpty()){
   QString dir=pending.takeLast();QFileInfo di(dir);
   if(!di.exists()||!di.isDir())continue;
   // Avoid following junctions and symbolic links into potentially huge trees.
   if(di.isSymLink())continue;
   QDir folder(dir);
   // Packages are recognized by their root manifest, without visiting
   // thousands of textures, BGLs and other files inside the package.
   if(QFileInfo::exists(folder.filePath("manifest.json"))){
    QString path=di.absoluteFilePath();
    if(!seen.contains(path)){
     seen.insert(path);result.push_back(identifyPackage(path));
    }
    continue;
   }
   // Enumerate subdirectories only; do not recursively enumerate files.
   QFileInfoList children=folder.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks|QDir::Hidden|QDir::System,QDir::NoSort);
   for(const QFileInfo &child:children)pending.push_back(child.absoluteFilePath());
   ++checked;
   if(checked%40==0 && self){
    int count=result.size();QString current=di.fileName();
    QMetaObject::invokeMethod(self.data(),[self,count,checked,current](){
      if(self && self->scanDetail)self->scanDetail->setText(QString("%1 packages trouvés • %2 dossiers examinés • %3").arg(count).arg(checked).arg(current));
    },Qt::QueuedConnection);
   }
  }
  return result;
 }));
}
 void scanFinished(){airports=watcher.result();for(auto &a:airports)if(overrides.contains(a.path)){a.icao=overrides.value(a.path);a.source="Validé manuellement";}validateSelections();render();QJsonArray list;for(const auto &a:airports)list.append(QJsonObject{{"name",a.name},{"icao_candidate",a.icao},{"icao_source",a.source},{"path",a.path}});QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);QSaveFile file(inventoryPath());if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonObject{{"read_only",true},{"packages",list}}).toJson());file.commit();}refreshButton->setEnabled(true);progress->hide();scanDetail->setText(QString("Analyse terminée : %1 packages").arg(airports.size()));info->setText(QString("%1 packages trouvés • Inventaire sauvegardé").arg(airports.size()));}
 void simbrief(){QString id=user->text().trimmed();if(id.isEmpty())return;QSettings("FlightSceneryManager","FlightSceneryManager").setValue("simbrief",id);QUrl url("https://www.simbrief.com/api/xml.fetcher.php");QUrlQuery q;q.addQueryItem(QRegularExpression("^\\d+$").match(id).hasMatch()?"userid":"username",id);url.setQuery(q);info->setText("Chargement SimBrief...");auto *reply=net.get(QNetworkRequest(url));connect(reply,&QNetworkReply::finished,this,[this,reply]{QByteArray data=reply->readAll();bool ok=reply->error()==QNetworkReply::NoError;QString error=reply->errorString();reply->deleteLater();if(!ok){QMessageBox::warning(this,"SimBrief",error);return;}QString a=xmlValue(data,"origin","icao_code"),b=xmlValue(data,"destination","icao_code");if(a.isEmpty()||b.isEmpty()){QMessageBox::warning(this,"SimBrief","Aucun OFP exploitable. Vérifie ton identifiant et génère un vol.");return;}from->setText(a);to->setText(b);alternate->setText(xmlValue(data,"alternate","icao_code"));QString seconds=xmlValue(data,"times","est_time_enroute");if(seconds.isEmpty())seconds=xmlValue(data,"times","sched_time_enroute");bool valid=false;int secs=seconds.toInt(&valid);durationLabel->setText(valid?QString("Durée prévue : %1 h %2 min").arg(secs/3600).arg((secs%3600)/60,2,10,QChar('0')):"Durée prévue : non disponible");suggest();info->setText("Plan chargé : "+a+" → "+b);});}
 public:Window(){setWindowTitle("Flight Scenery Manager 0.4.1");resize(1350,850);setMinimumSize(950,650);auto *layout=new QVBoxLayout(this);layout->setContentsMargins(26,22,26,20);layout->setSpacing(14);auto *heading=new QLabel("✈  FLIGHT SCENERY MANAGER");heading->setObjectName("heading");layout->addWidget(heading);auto *sizeRow=new QHBoxLayout;sizeRow->addWidget(new QLabel("PRÉPARATION DU VOL   /   INVENTAIRE AUTOMATIQUE   /   MODE LECTURE SEULE"));sizeRow->addStretch();sizeRow->addWidget(new QLabel("Texte :"));fontChoice=new QComboBox;for(int n:{14,16,19,22})fontChoice->addItem(QString::number(n)+" px",n);sizeRow->addWidget(fontChoice);layout->addLayout(sizeRow);
 auto *top=new QHBoxLayout;user=new QLineEdit;user->setPlaceholderText("Identifiant SimBrief");auto *fetch=new QPushButton("↻ Charger le dernier vol");top->addWidget(user,2);top->addWidget(fetch);layout->addLayout(top);auto *route=new QHBoxLayout;from=new QLineEdit;to=new QLineEdit;from->setPlaceholderText("Départ ICAO");to->setPlaceholderText("Destination ICAO");route->addWidget(from);route->addWidget(new QLabel("→"));route->addWidget(to);alternate=new QLineEdit;alternate->setPlaceholderText("Dégagement ICAO");route->addWidget(alternate);layout->addLayout(route);durationLabel=new QLabel("Durée prévue : —");layout->addWidget(durationLabel);
 layout->addWidget(new QLabel("AÉROPORTS PROPOSÉS POUR LE VOL"));matches=new QTableWidget(0,4);matches->setHorizontalHeaderLabels({"Choix","ICAO","Scène locale","État"});matches->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);matches->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(matches,1);
 libraryTitle=new QLabel("BIBLIOTHÈQUE DE SCÈNES");layout->addWidget(libraryTitle);table=new QTableWidget(0,4);table->setHorizontalHeaderLabels({"ICAO","Choix","Package","Emplacement"});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table,2);
 auto *bottom=new QHBoxLayout;auto *add=new QPushButton("Sel. Dossier Scènes");auto *refresh=new QPushButton("↻ Actualiser");refreshButton=refresh;bottom->addWidget(add);bottom->addWidget(refresh);bottom->addStretch();layout->addLayout(bottom);
 progress=new QProgressBar;progress->setTextVisible(false);progress->setFixedHeight(7);progress->hide();layout->addWidget(progress);
 scanDetail=new QLabel("Inventaire prêt");layout->addWidget(scanDetail);info=new QLabel("Aucun fichier de scène ne sera modifié.");layout->addWidget(info);
 auto applyFont=[this](int px){setStyleSheet("QWidget{background:#0b1423;color:#e4f1ff;font:%1px 'Segoe UI'} QLabel#heading{font-size:%2px;font-weight:800;color:#5bd9f5} QPushButton{background:#19435d;color:#ecfaff;border:1px solid #3c7897;border-radius:8px;padding:12px;font-weight:700} QPushButton:hover{background:#28617e} QLineEdit{background:#12263b;border:1px solid #38556f;border-radius:7px;padding:11px} QTableWidget{background:#12243a;alternate-background-color:#172c43;border:1px solid #2c4864;gridline-color:#203950} QHeaderView::section{background:#1d3852;color:#a5c9df;padding:9px;border:0} QProgressBar{background:#173048;border:0;border-radius:3px} QProgressBar::chunk{background:#5bd9f5;border-radius:3px} QComboBox{background:#12263b;border:1px solid #38556f;border-radius:7px;padding:8px}").arg(px).arg(px+13));
 table->verticalHeader()->setDefaultSectionSize(px+22);matches->verticalHeader()->setDefaultSectionSize(px+22);QSettings("FlightSceneryManager","FlightSceneryManager").setValue("fontSize",px);};int saved=QSettings("FlightSceneryManager","FlightSceneryManager").value("fontSize",16).toInt();int idx=fontChoice->findData(saved);fontChoice->setCurrentIndex(idx<0?1:idx);applyFont(fontChoice->currentData().toInt());connect(fontChoice,&QComboBox::currentIndexChanged,this,[this,applyFont](int){applyFont(fontChoice->currentData().toInt());});
 QSettings settings("FlightSceneryManager","FlightSceneryManager");roots=settings.value("roots").toStringList();user->setText(settings.value("simbrief").toString());for(const QString &p:settings.value("selectedPaths").toStringList())selectedPaths.insert(p);auto overrideMap=settings.value("icaoOverrides").toMap();for(auto it=overrideMap.cbegin();it!=overrideMap.cend();++it)overrides.insert(it.key(),it.value().toString());connect(fetch,&QPushButton::clicked,this,[this]{simbrief();});connect(refresh,&QPushButton::clicked,this,[this]{scan();});connect(add,&QPushButton::clicked,this,[this]{QString d=QFileDialog::getExistingDirectory(this,"Choisir le dossier des scènes");if(!d.isEmpty()&&!roots.contains(d)){roots<<d;QSettings("FlightSceneryManager","FlightSceneryManager").setValue("roots",roots);scan();}});connect(from,&QLineEdit::textChanged,this,[this]{suggest();});connect(to,&QLineEdit::textChanged,this,[this]{suggest();});connect(alternate,&QLineEdit::textChanged,this,[this]{suggest();});connect(table,&QTableWidget::cellDoubleClicked,this,[this](int row,int column){if(column!=0||row<0||row>=airports.size())return;const Airport a=airports.at(row);if(a.source.startsWith("Plusieurs ICAO")){QMessageBox::warning(this,"Package multi-aéroports","Plusieurs ICAO détectés : ce package ne peut pas être réduit à un seul aéroport. Sélection désactivée jusqu’à prise en charge des packages multi-aéroports.");return;}bool ok=false;QString value=QInputDialog::getText(this,"Validation ICAO","Code ICAO de la scène (4 lettres) :",QLineEdit::Normal,a.icao,&ok).trimmed().toUpper();if(!ok)return;if(!QRegularExpression("^[A-Z]{4}$").match(value).hasMatch()){QMessageBox::warning(this,"ICAO","Le code doit contenir exactement quatre lettres.");return;}overrides[a.path]=value;QVariantMap map;for(auto it=overrides.cbegin();it!=overrides.cend();++it)map.insert(it.key(),it.value());QSettings("FlightSceneryManager","FlightSceneryManager").setValue("icaoOverrides",map);airports[row].icao=value;airports[row].source="Validé manuellement";validateSelections();render();});connect(table,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *item){if(!filling&&item->column()==1)choose(item->data(Qt::UserRole).toString(),item->checkState()==Qt::Checked);});connect(matches,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *item){if(!filling&&item->column()==0)choose(item->data(Qt::UserRole).toString(),item->checkState()==Qt::Checked);});connect(&watcher,&QFutureWatcher<QVector<Airport>>::finished,this,[this]{scanFinished();});loadCache();}
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setOrganizationName("FlightSceneryManager");app.setApplicationName("FlightSceneryManager");Window window;window.show();return app.exec();}
#include "main.moc"
