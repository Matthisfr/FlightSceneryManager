# Flight Scenery Manager — V0.4.0 (prototype Windows)

Gestionnaire de bibliothèques de scènes pour MSFS 2024, en C++/Qt 6.

## Fonctions disponibles
- Analyse des dossiers de packages MSFS sans parcourir le contenu de chaque package après détection de manifest.json.
- Analyse en arrière-plan avec indicateur d'activité et inventaire mis en cache au redémarrage.
- Identification indicative d'ICAO par noms explicites de BGL, titre du manifeste et nom de dossier. **Ces indices ne prouvent pas l'ICAO réellement contenu dans un BGL.**
- Validation manuelle d'un ICAO : double-cliquer sur sa cellule dans la bibliothèque, puis saisir le code de quatre lettres. Les corrections sont mémorisées par chemin de package.
- Sélection d'au plus un package par ICAO **validé manuellement**. Les choix sont mémorisés. Les packages non validés ne sont pas sélectionnables.
- Chargement du dernier vol SimBrief : départ, destination, dégagement et durée estimée si disponible.
- Réglage persistant de la taille des textes.

## Limitations de sécurité
- **Lecture seule : aucune activation, désactivation, copie, déplacement ou suppression de scènes.** Les cases représentent des préférences, pas l'état actif de MSFS.
- L'analyseur ne décode pas encore les données internes des BGL, ne détecte pas les aéroports multiples dans un package et ne garantit pas l'absence de conflits réels.
- Les codes validés manuellement restent sous la responsabilité de l'utilisateur.
- Les dossiers de packages accessibles par liens symboliques ne sont pas parcourus.

## Télécharger
Dans GitHub Actions, ouvrir la dernière exécution réussie du workflow **Windows EXE**, puis télécharger l'artifact **FlightSceneryManager-V0.4.0-Windows-x64**. Décompresser et lancer FlightSceneryManager.exe. Une compilation réussie doit être confirmée avant utilisation.
