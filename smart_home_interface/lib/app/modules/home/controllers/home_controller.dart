import 'dart:async';
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:get/get.dart';
import 'package:smart_home/app/data/models/room_model.dart';
import 'package:smart_home/app/modules/connected_device/views/connected_device_view.dart';
import 'package:smart_home/app/modules/home/views/dashboard_view.dart';
import 'package:smart_home/app/modules/home/views/settings_view.dart';

class HomeController extends GetxController {
  // bottom nav current index.
  RxInt _currentIndex = 0.obs;
  get currentIndex => this._currentIndex.value;

  // userData
  String userName = 'Jobin';
  bool isMale = true;

  // List of bools for selected room
  List<bool> selectedRoom = [true, false, false, false, false];

  // the list of screens switched by bottom navBar
  final List<Widget> homeViews = [
    DashboardView(),
    ConnectedDeviceView(),
    SettingsView(),
  ];

  // List of room data
  List<Room> rooms = [
    Room(roomName: 'Living Room', roomImgUrl: 'assets/icons/sofa.svg'),
    Room(roomName: 'Dining Room', roomImgUrl: 'assets/icons/chair.svg'),
    Room(roomName: 'Bedroom', roomImgUrl: 'assets/icons/bed.svg'),
    Room(roomName: 'Kitchen', roomImgUrl: 'assets/icons/kitchen.svg'),
    Room(roomName: 'Bathroom', roomImgUrl: 'assets/icons/bathtub.svg'),
  ];

  List<bool> isToggled = [false, false, false, false];

  RxInt temperature = 25.obs;
  RxInt humidity = 56.obs;

  // store current color for UI only
  RxString currentRGB = '#ffffff'.obs;
  RxString newRGB = '#ffffff'.obs;

  Timer? _sensorTimer;

  // function to set current index
  setCurrentIndex(int index) {
    _currentIndex.value = index;
  }

  // function to return correct view on bottom navBar switch
  Widget navBarSwitcher() {
    return homeViews.elementAt(currentIndex);
  }

  // function to move between each room
  void roomChange(int index) {
    selectedRoom = [false, false, false, false, false];
    selectedRoom[index] = true;
    update([1, true]);
  }

  // switches in the room
  onSwitched(int index) {
    isToggled[index] = !isToggled[index];
    if (index == 1) {
      currentRGB.value = isToggled[index] ? '#ffffff' : '#000000';
      newRGB.value = currentRGB.value;
    }
    update([2, true]);
  }

  sendRGBColor(String hex) {
    currentRGB.value = hex;
    newRGB.value = hex;
    update([2, true]);
  }

  void simulateSensorValues() {
    _sensorTimer = Timer.periodic(Duration(seconds: 3), (_) {
      temperature.value = 22 + Random().nextInt(7);
      humidity.value = 45 + Random().nextInt(30);
    });
  }

  @override
  void onInit() {
    super.onInit();
    simulateSensorValues();
  }

  @override
  void onClose() {
    _sensorTimer?.cancel();
    super.onClose();
  }
}
