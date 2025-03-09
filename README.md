# SummonerVR
 
# RPG VR Game for Meta Quest 3

## Overview
This is an RPG VR game developed in Unreal Engine 5 for the Meta Quest 3, utilizing the **Gameplay Ability System (GAS)** for ability management and **miVRy Plugin** for gesture-based spellcasting. The game is inspired by the *Summoner* book series by **Taran Matharu** and modifies concepts from the course *Unreal Engine 5 - Gameplay Ability System - Top Down RPG*, adapting them for VR.

## Features
### Player Avatar
- **Metahuman Setup**: The player avatar is a Metahuman animated using the **VRIKBody Plugin**.
- **Hand Animation Blending**: Custom hand animations blended made using **Sequencer**.

### Gesture-Based Magic System
- **miVRy Plugin Integration**: Recognizes hand gestures to cast spells.
- **Three Magic Abilities Implemented**:
  - **Firebolt** (Lighting-shape Gesture)
  - **Firebeam** (M-shape Gesture)
  - **Wyrdlight** (Circle Gesture)
- **Gameplay Ability System (GAS)**: Abilities and inputs are tagged, allowing to binf the gesture-recognized for abilities by miVRy to input actions (e.g., Oculus RT for magic casting).

### Enemy AI
- **Three Enemy Types**: Ranger, Summoner, and Warrior.
- **AI Attack System Completed**.
- **Behavior Trees & EQS**: Implemented custom AI behaviors using Behavior Trees and the Environment Query System (EQS).

### RPG Mechanics
#### Leveling System
- Interactive **VR level-up menu** to allocate attribute points.
- **Primary Attributes**:
  - Strength (increases energy and physical damage)
  - Intelligence (increases magical damage)
  - Defence (increases Armor and Armor Penetration)
  - Vigor (increases Max Health)
- **Secondary Attributes** (derived from primary attributes and custom variables):
  - Armor (reduces damage taken, improves Block Chance)
  - Armor Penetration (ignores percentage of enemy Armor, increases Critical Hit Chance)
  - Block Chance (chance to cut incoming damage in half)
  - Critical Hit Chance (chance to double damage plus critical hit bonus)
  - Critical Hit Damage (bonus damage added when a critical hit is scored)
  - Critical Hit Resistance (reduces critical hit chance of attacking enemies)
  - Health, Mana, and Energy Regeneration
  - Max Health, Mana, and Energy

### VR-Specific Enhancements
- **3D UI Widgets for Combat Feedback**:
  - Floating text for damage numbers, color-coded based on attack type.
  - Floating text for **critical hits**, **blocked hits**, and **critical blocked hits**.

## Plugins Used
- [**miVRy Plugin**](https://www.marui-plugin.com/mivry/) - Gesture recognition for magic casting.
- **VRIKBody Plugin** - Full-body Metahuman VR animation.
- **Gameplay Ability System (GAS)** - Ability management framework.

## Future Plans
- Change UI to be more fit for VR games.
- Add more magic abilities with unique gestures.
- Enhance interaction with the world using immersive VR mechanics.

---
This project is a passion-driven attempt to bring *Summoner* into an immersive VR RPG experience. Contributions, feedback, and ideas are always welcome!

