#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
namespace fs = std::filesystem;
std::string esc(const std::string& s) {
 std::string o;
 for(char c:s) {
  if(c=='"'||c=='\\'){o+='\\';o+=c;}
  else if(c=='\n')o+="\\n";
  else if((unsigned char)c<32)o+=' ';
  else o+=c;
 }
 return o;
}
struct Package {std::string path,name,manifest,type;bool link;};
int main(int argc,char**argv) {
 if(argc<2) {
  std::cout<<"Flight Scenery Manager 0.1 - INVENTAIRE SEUL (aucune modification)\nUsage: FlightSceneryManager.exe <dossier1> [dossier2 ...]\n";
  return 0;
 }
 std::vector<Package> found;
 std::error_code ec;
 for(int a=1;a<argc;++a) {
  fs::path root=fs::u8path(argv[a]);
  if(!fs::exists(root,ec)) {std::cerr<<"Dossier introuvable: "<<argv[a]<<"\n";continue;}
  fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
  while(it!=end) {
   if(ec){ec.clear();it.increment(ec);continue;}
   auto p=it->path();
   if(it->is_directory(ec)) {
    fs::path m=p/"manifest.json";
    if(fs::is_regular_file(m,ec)) {
     Package pkg;
     pkg.path=p.u8string();
     pkg.name=p.filename().u8string();
     pkg.manifest=m.u8string();
     pkg.link=fs::is_symlink(p,ec);
     pkg.type="package MSFS (type et ICAO non verifies)";
     found.push_back(pkg);
     it.disable_recursion_pending();
    }
   }
   it.increment(ec);
  }
 }
 std::ofstream out("inventory.json",std::ios::binary|std::ios::trunc);
 if(!out){std::cerr<<"Impossible d'ecrire inventory.json\n";return 2;}
 out<<"{\n  \"schema\": 1,\n  \"mode\": \"read-only\",\n  \"packages\": [\n";
 for(size_t i=0;i<found.size();++i) {
  const auto& p=found[i];
  out<<"    {\"name\":\""<<esc(p.name)<<"\",\"path\":\""<<esc(p.path)<<"\",\"manifest\":\""<<esc(p.manifest)<<"\",\"is_symlink\":"<<(p.link?"true":"false")<<",\"classification\":\""<<p.type<<"\"}"<<(i+1<found.size()?",":"")<<"\n";
 }
 out<<"  ]\n}\n";
 out.close();
 std::cout<<found.size()<<" package(s) detecte(s). Inventaire: inventory.json\nAucun fichier MSFS n'a ete modifie.\n";
 return 0;
}
