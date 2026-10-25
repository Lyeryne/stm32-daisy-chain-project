# Système de Communication en Daisy-Chain (STM32)

Ce dépôt contient le code source d'un projet d'Architecture des Systèmes Embarqués développé sur microcontrôleurs STM32. 

L'objectif du projet est de concevoir un système de transmission de proche en proche (daisy-chain) garantissant l'intégrité et la confidentialité des données échangées entre plusieurs nœuds.

## 🛠️ Architecture Matérielle
* **Microcontrôleurs** : 2x cartes STMicroelectronics NUCLEO-F446RE.
* **Communication** : UART (Configuration croisée TX/RX entre les nœuds).
* **Interface** : Utilisation du bouton poussoir (USER_BUTTON) et de la LED verte intégrés à la carte Nucleo.

## 🚀 Fonctionnalités Principales
Le système implémente une communication sécurisée Maître-Esclave avec les caractéristiques suivantes :

* **Initialisation et Synchronisation** : 
  * Au démarrage, les nœuds sont en attente (`STATE_UNINITIALIZED`).
  * L'appui sur le bouton du Nœud 1 (Maître) déclenche la génération d'une clé cryptographique 32 bits (`0xDEADBEEF`).
  * Envoi d'une trame `CMD_INIT` vers le Nœud 2 (Esclave).
  * Le Nœud 2 accuse réception (`CMD_ACK`) et les deux cartes passent en mode `STATE_SYNCHRONIZED` (LED verte fixe).
* **Sécurité des données** :
  * **Intégrité** : Vérification par somme de contrôle CRC-16 intégrée à chaque trame de communication.
  * **Confidentialité** : Chiffrement symétrique des messages (`CMD_MSG`) basé sur un XOR avec la clé partagée lors de l'initialisation.

## 📁 Structure du Dépôt
Le projet a été généré via STM32CubeMX et est développé sous STM32CubeIDE. Il regroupe les deux nœuds communicants :

* `STM32_Node1_Ayman_Louaye/` : Code source du Nœud 1 (Comportement Maître).
* `STM32_Node2_Ayman_Louaye/` : Code source du Nœud 2 (Comportement Esclave).
* `protocol_icd.h` : Fichier d'en-tête commun définissant la structure des trames (Header, Payload, CRC) et les états du système.

Chaque dossier de nœud contient les répertoires standards `Core/` (logique applicative) et `Drivers/` (bibliothèques HAL/CMSIS). Les dossiers de compilation (`Debug/`, `Release/`) sont volontairement ignorés.

## 👥 Auteurs
* Ayman OUGUERD
* Louaye SAGHIR

Projet réalisé dans le cadre du module d'Architecture des Systèmes Embarqués.