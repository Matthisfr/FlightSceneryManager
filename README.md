# Flight Scenery Manager — V0.1

Prototype C++17 autonome, **lecture seule** : ne modifie aucun package ni lien Windows.

## Fonctionnement
- Analyse les dossiers fournis en arguments à chaque exécution (donc détecte les nouveaux packages au prochain lancement).
- Recherche les dossiers contenant `manifest.json`.
- Exporte `inventory.json` dans le dossier de travail.
- Ne détermine pas encore les ICAO, les dépendances ni les changements entre deux scans.
- Pas encore d'interface Qt, de SimBrief, de surveillance en temps réel ou d'activation/désactivation.

## Télécharger l'EXE Windows
Ouvrir **Actions > Windows EXE**. Après un lancement réussi, ouvrir le run puis télécharger l'artifact **FlightSceneryManager-Windows-x64**. Le build est automatique à chaque push sur `main`, et peut aussi être lancé via **Run workflow**.

Dans une invite de commandes Windows :
```bat
FlightSceneryManager.exe "D:\\MSFS_Addons" "D:\\AutresScenes"
```

L'inventaire contient des chemins locaux : ne pas le publier. Ne pas analyser de dossiers sensibles.

## Feuille de route
1. Interface Qt 6 et sélection des dossiers.
2. Comparaison d'inventaires et détection des ajouts, suppressions, modifications.
3. Détection ICAO fiable et reconnaissance des liens Windows / Addons Linker.
4. SimBrief et simulation d'activation.
5. Gestion réelle avec sauvegardes et restauration, après validation.
