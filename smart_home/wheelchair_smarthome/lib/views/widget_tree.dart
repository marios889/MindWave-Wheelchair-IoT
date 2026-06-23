import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/notifiers/notifiers.dart';
import 'package:wheelchair_smarthome/views/pages/rooms_page.dart';
import 'package:wheelchair_smarthome/views/pages/settings_page.dart';
import 'package:wheelchair_smarthome/views/pages/wheelchair_page.dart';
import 'package:wheelchair_smarthome/views/widgets/navbar_widget.dart';

List<Widget> pages = [WheelchairPage(), RoomsPage(), SettingsPage()];
List<String> titles = ["Wheelchair", "Rooms", "Settings"];

class WidgetTree extends StatelessWidget {
  const WidgetTree({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: ValueListenableBuilder(
          valueListenable: selectedPageNotifier,
          builder: (context, selectedPage, child) {
            return Text(titles[selectedPage]);
          },
        ),
        centerTitle: true,
      ),
      body: ValueListenableBuilder(
        valueListenable: selectedPageNotifier,
        builder: (context, selectedPage, child) {
          return pages.elementAt(selectedPage);
        },
      ),
      bottomNavigationBar: NavbarWidget(),
    );
  }
}
