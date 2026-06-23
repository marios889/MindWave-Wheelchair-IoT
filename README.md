# EmbedInt: Multi-Modal Smart Wheelchair & IoT Ecosystem ♿🏡

![Project Status](https://img.shields.io/badge/Status-Completed-success)
![Platform](https://img.shields.io/badge/Platform-ESP32%20%7C%20Wemos%20D1-blue)
![Network](https://img.shields.io/badge/Network-UDP%20%7C%20ESP--NOW-orange)

**EmbedInt** is a commercially scalable, decentralized assistive ecosystem designed to restore mobility and environmental autonomy to elderly users and individuals with severe motor disabilities. 

This project couples a 24V closed-loop smart wheelchair with a local Internet-of-Things (IoT) smart home environment. By entirely bypassing cloud dependency, EmbedInt ensures zero-latency execution, total privacy, and offline reliability through a custom User Datagram Protocol (UDP) framework.

---

## ✨ Key Features & Multi-Modal Control

To ensure universal accessibility, the system adapts to the physical capabilities of the user through a triple-redundant control architecture:

* 🧠 **Brain-Computer Interface (BCI):** Utilizes a single-channel NeuroSky EEG headset. 
    * **Ocular Navigation:** Custom Schmitt-Trigger extraction isolates intentional eye blinks from ambient noise (featuring a 250ms software debounce) achieving 96% accuracy.
    * **The "Nuclear Option":** Repurposes high-amplitude electromyographic (EMG) jaw-clench artifacts as a deterministic, zero-latency emergency hardware brake.
    * **Cognitive Handshake:** System safely locks until a biometric focus threshold (`Attention ≥ 50`) is met.
* 🗣️ **Offline Voice Control:** AI-Thinker VC-02 module provides localized, internet-free voice command translation.
* 📱 **Supervisory Dashboard:** A responsive Flutter mobile application for caregiver oversight and manual network control.

## 🛠️ Hardware Architecture & Engineering Solutions

Building a safe, 24V motorized system required solving complex hardware bottlenecks:

### 1. Architectural Radio Decoupling
Attempting to multiplex Bluetooth Classic (for the EEG headset) and ESP-NOW (for proximity) on a single ESP32 antenna caused catastrophic stack contention. **Solution:** The architecture was decoupled. A dedicated Wemos D1 Mini + HC-05 gateway processes all biometric and proximity telemetry, forwarding normalized commands to the main ESP32 mobility controller via low-latency UDP.

### 2. Closed-Loop Kinematics
Open-loop control of the 24V DC motors resulted in severe trajectory drift. **Solution:** Upgraded from inertial sensor fusion (MPU-6050) to a dual Rotary-Encoder Proportional-Integral-Derivative (PID) closed-loop system, increasing straight-line locomotion efficiency from 85% to 95%.

### 3. Decentralized IoT Nodes
The smart home environment utilizes independent ESP-12F nodes equipped with non-blocking firmware to ensure physical wall switches remain operational even if the Wi-Fi network drops. 
* **Actuators:** 10A/30A Relays, L293D motorized curtains with absolute encoders.
* **Sensors:** MQ-5 combustible-gas safety monitoring.

## 📡 Network Protocol

EmbedInt operates on a strictly local-area network to prioritize deterministic safety:
* **Transport Layer:** Connectionless UDP on Port 7777.
* **Packet Loss Mitigation:** Employs a custom "triple-send" redundant transmission scheme with idempotent receiver logic, suppressing packet loss from ~25% to 0%.
* **Dynamic Discovery:** IoT nodes dynamically learn the supervisory controller's IP via broadcast datagrams, eliminating hard-coded network dependencies.

## 🔬 The Machine Learning Pivot

*An important note for developers reviewing this repository:* Initial prototyping utilized a Random Forest classifier to decode motor imagery. However, real-world testing revealed severe **Electromyographic (EMG) Data Leakage**—the single Fp1 electrode was inadvertently classifying facial muscle shifts rather than localized motor cortex brainwaves. 

To guarantee the deterministic safety required for a heavy motorized vehicle, we intentionally abandoned the probabilistic ML models in favor of a strict, rule-based state machine that perfectly leverages those exact high-amplitude physiological artifacts.

## 🎓 Academic Context

This system was engineered as a Graduation Project for the **Faculty of Engineering at Shoubra, Benha University (Class of 2026)**.
