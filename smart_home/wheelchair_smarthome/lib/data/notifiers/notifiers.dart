import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';

ValueNotifier<bool> isDarkModeNotifier = ValueNotifier(false);

ValueNotifier<int> selectedPageNotifier = ValueNotifier(0);
ValueNotifier<String> WheelchairNotifier = ValueNotifier("Unknown");
ValueNotifier<int> WheelchairPortNotifier = ValueNotifier(8000);
ValueNotifier<String> WheelchairSpeedNotifier = ValueNotifier("N/A");
ValueNotifier<bool> WheelchairisConnected = ValueNotifier(false);
ValueNotifier<double> speedNotifier = ValueNotifier<double>(0.5);

class SensorNodeNotifiers {
  static ValueNotifier<String> tempNotifier = ValueNotifier("0");
  static ValueNotifier<String> humidNotifier = ValueNotifier("0");
  static ValueNotifier<String> MethaneNotifier = ValueNotifier("0");
  static ValueNotifier<String> CO2Notifier = ValueNotifier("0");
  static ValueNotifier<bool> MethaneAlert = ValueNotifier(false);
  static ValueNotifier<bool> CO2Alert = ValueNotifier(false);
}

class DoorNodeNotifiers {
  static ValueNotifier<bool> DoorStateNotifier = ValueNotifier(false);
}

class PIRNodeNotifiers {
  static ValueNotifier<bool> PIRStateNotifier = ValueNotifier(false);
}
// inside your wheelchair_controller.dart

class WheelchairController {
  // 💡 Initialize the notifier strictly at the lower bound (0.2)
  static final ValueNotifier<double> speedKmH = ValueNotifier<double>(0.2);

  // Define exact explicit boundary constraints
  static const double minSpeedLimit = 0.2;
  static const double maxSpeedLimit = 1.0;
  static const double speedStep = 0.2;

  static void increaseSpeed() {
    // A tiny epsilon deduction (- 0.01) guarantees floating point comparisons work flawlessly
    if (speedKmH.value < maxSpeedLimit - 0.01) {
      double nextSpeed = speedKmH.value + speedStep;

      // Fixes the binary precision bug (e.g., prevents 0.60000000001)
      speedKmH.value = double.parse(nextSpeed.toStringAsFixed(1));

      UdpService.sendUDP(
        "I",
        WheelchairNotifier.value,
        WheelchairPortNotifier.value,
      );
    }
  }

  static void decreaseSpeed() {
    // A tiny epsilon addition (+ 0.01) protects the minimum boundary check
    if (speedKmH.value > minSpeedLimit + 0.01) {
      double nextSpeed = speedKmH.value - speedStep;

      // Fixes the binary precision bug
      speedKmH.value = double.parse(nextSpeed.toStringAsFixed(1));

      UdpService.sendUDP(
        "D",
        WheelchairNotifier.value,
        WheelchairPortNotifier.value,
      );
    }
  }
}
