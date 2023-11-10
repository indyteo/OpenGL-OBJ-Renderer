# OpenGL-OBJ-Renderer
Projet réalisé en cours d'OpenGL en E4FI à ESIEE Paris.

Membres du groupe :
- [Théo SZANTO](https://github.com/indyteo)
- [Jenny CAO](https://github.com/jennycjay)
- [Joe BERTHELIN](https://github.com/jo3pro)

![Aperçu du rendu de l'application](Rendu.gif)

## Description

Le but du projet est d'afficher des objets 3D à partir de fichiers `.obj` en utilisant des shaders différents, et en prenant en charge les matériaux ainsi que l'illumination. Il est possible de déplacer la caméra dans la scène 3D.

## Utilisation

Ce projet a été développé dans un environnement Windows.

Le code du projet se trouve dans le dossier `Projet`, les librairies dans `libs`, et quelques fichires annexes dans `common`. Les librairies ont été incluses pour plus de simplicité.
Après avoir cloné le repository, copiez `glew32.dll` dans le dossier de build de CMake du dossier `Projet`. Vous devriez pouvoir exécuter CMake pour build le projet.
