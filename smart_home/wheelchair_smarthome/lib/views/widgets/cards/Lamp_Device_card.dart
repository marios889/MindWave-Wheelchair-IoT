import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/popupmenu_dialog.dart';

class LampDeviceCard extends StatefulWidget {
  final SmartDevice device;
  final VoidCallback? onTap;

  const LampDeviceCard({super.key, required this.device, this.onTap});

  @override
  State<LampDeviceCard> createState() => _LampDeviceCardState();
}

class _LampDeviceCardState extends State<LampDeviceCard> {
  // Helper to assign a matching icon based on your DeviceType enum
  String _getDeviceIcon(DeviceType type) {
    switch (type) {
      case DeviceType.lamp:
        return "assets/images/idea-bulb.png";
      case DeviceType.curtains:
        return "assets/images/smart-curtain-background.png";
      case DeviceType.sensorNode:
        return "assets/images/humidity.png";
      case DeviceType.Device:
        return "assets/images/smart-grid.png";
      case DeviceType.DoorContact:
        return "assets/images/smart-door.png";
      case DeviceType.lamp_device:
        return "assets/images/plug.png";
      case DeviceType.PIRsensor:
        return "assets/images/plug.png";
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return ValueListenableBuilder(
      valueListenable: widget.device.isOnline,
      builder: (context, isOnline, child) {
        return InkWell(
          onTap: isOnline ? widget.onTap : null,
          onLongPress: () =>
              PopupmenuDialog.showPopupMenu(context, widget.device),
          borderRadius: BorderRadius.circular(20.0),

          child: ValueListenableBuilder(
            valueListenable: widget.device.isOn30A,
            builder: (context, powerState30A, child) {
              return ValueListenableBuilder(
                valueListenable: widget.device.isOn,
                builder: (context, powerState, child) {
                  // ⚡ 3. Dynamic Styling Calculations based on Online & Power States
                  final Color cardBackground = !isOnline
                      ? theme.colorScheme.surfaceContainerHighest.withValues(
                          alpha: 0.4,
                        ) // Disconnected Gray Look
                      : powerState || powerState30A
                      ? theme.colorScheme.primary.withValues(alpha: 0.08)
                      : theme.colorScheme.surface;

                  final Color cardBorderColor = !isOnline
                      ? theme.colorScheme.onSurface.withValues(alpha: 0.1)
                      : powerState || powerState30A
                      ? theme.colorScheme.primary.withValues(alpha: 0.4)
                      : theme.colorScheme.onSurface.withValues(alpha: 0.25);
                  return AnimatedContainer(
                    duration: const Duration(milliseconds: 200),
                    padding: const EdgeInsets.all(16.0),
                    decoration: BoxDecoration(
                      borderRadius: BorderRadius.circular(20.0),
                      // Subtle color change if the device is turned on
                      color: cardBackground,

                      border: Border.all(color: cardBorderColor, width: 1.5),
                      boxShadow: [
                        if ((powerState || powerState30A) && isOnline)
                          BoxShadow(
                            color: theme.colorScheme.primary.withValues(
                              alpha: 0.1,
                            ),
                            blurRadius: 8,
                            offset: const Offset(0, 4),
                          ),
                      ],
                    ),
                    child: Padding(
                      padding: const EdgeInsets.symmetric(vertical: 6.0),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,

                        children: [
                          // TOP ROW: Icon and Toggle Control
                          AbsorbPointer(
                            absorbing: !isOnline,
                            child: Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              children: [
                                // Clean circular badge for device icon
                                Container(
                                  padding: const EdgeInsets.all(8),
                                  decoration: BoxDecoration(
                                    color: !isOnline
                                        ? Colors.white.withValues(
                                            alpha: 0.15,
                                          ) // Desaturated circle background
                                        : powerState || powerState30A
                                        ? Colors.white.withValues(alpha: 0.9)
                                        : Colors.white.withValues(alpha: 0.35),
                                    shape: BoxShape.circle,
                                  ),
                                  child: SizedBox(
                                    width: 50.0,
                                    height: 50.0,

                                    child: Opacity(
                                      opacity: isOnline ? 1.0 : 0.4,
                                      child: Image.asset(
                                        _getDeviceIcon(DeviceType.Device),

                                        fit: BoxFit.cover,
                                      ),
                                    ),
                                  ),
                                ),

                                // Toggle Switch for hardware control
                                Switch(
                                  value: isOnline
                                      ? powerState
                                      : false, // Force false if offline
                                  thumbColor:
                                      WidgetStateProperty.resolveWith<Color?>((
                                        states,
                                      ) {
                                        if (!isOnline) {
                                          return theme.colorScheme.outline;
                                        }
                                        // Gray thumb if offline
                                        if (states.contains(
                                          WidgetState.selected,
                                        )) {
                                          return theme.colorScheme.primary;
                                        }
                                        return null;
                                      }),
                                  trackColor:
                                      WidgetStateProperty.resolveWith<Color?>((
                                        states,
                                      ) {
                                        if (!isOnline) {
                                          return theme
                                              .colorScheme
                                              .surfaceContainerHighest;
                                        }
                                        // Gray track if offline
                                        if (states.contains(
                                          WidgetState.selected,
                                        )) {
                                          return theme.colorScheme.primary
                                              .withValues(alpha: 0.4);
                                        }
                                        return null;
                                      }),
                                  onChanged: (value) async {
                                    DevicesState.toggleDeviceStatus(
                                      widget.device.id,
                                      value,
                                    );
                                    for (int i = 0; i < 3; i++) {
                                      UdpService.sendUDP(
                                        value ? "O10" : "C10",
                                        widget.device.ip,
                                        int.parse(widget.device.port),
                                        useWatchdog: true,
                                        timeoutDuration: const Duration(
                                          seconds: 2,
                                        ),
                                      );
                                      await Future.delayed(
                                        const Duration(milliseconds: 100),
                                      );
                                    }
                                  },
                                ),
                              ],
                            ),
                          ),

                          AbsorbPointer(
                            absorbing: !isOnline,
                            child: Row(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              children: [
                                Text("30A Device"),
                                Switch(
                                  value: isOnline
                                      ? powerState30A
                                      : false, // Force false if offline
                                  thumbColor:
                                      WidgetStateProperty.resolveWith<Color?>((
                                        states,
                                      ) {
                                        if (!isOnline) {
                                          return theme.colorScheme.outline;
                                        }
                                        // Gray thumb if offline
                                        if (states.contains(
                                          WidgetState.selected,
                                        )) {
                                          return theme.colorScheme.primary;
                                        }
                                        return null;
                                      }),
                                  trackColor:
                                      WidgetStateProperty.resolveWith<Color?>((
                                        states,
                                      ) {
                                        if (!isOnline) {
                                          return theme
                                              .colorScheme
                                              .surfaceContainerHighest;
                                        }
                                        // Gray track if offline
                                        if (states.contains(
                                          WidgetState.selected,
                                        )) {
                                          return theme.colorScheme.primary
                                              .withValues(alpha: 0.4);
                                        }
                                        return null;
                                      }),
                                  onChanged: (value) async {
                                    DevicesState.toggleDevice30AStatus(
                                      widget.device.id,
                                      value,
                                    );
                                    for (int i = 0; i < 3; i++) {
                                      UdpService.sendUDP(
                                        value ? "O30" : "C30",
                                        widget.device.ip,
                                        int.parse(widget.device.port),
                                        useWatchdog: true,
                                        timeoutDuration: const Duration(
                                          seconds: 2,
                                        ),
                                      );
                                      await Future.delayed(
                                        const Duration(milliseconds: 100),
                                      );
                                    }
                                  },
                                ),
                              ],
                            ),
                          ),

                          // BOTTOM SECTION: Device Meta-data (Name & IP Address)
                          Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              SizedBox(height: 10.0),
                              Row(
                                children: [
                                  Container(
                                    width: 10.0,
                                    height: 10.0,
                                    decoration: BoxDecoration(
                                      color: isOnline
                                          ? Colors.greenAccent[700]
                                          : Colors.redAccent[700],
                                      shape: BoxShape.circle,
                                      boxShadow: [
                                        BoxShadow(
                                          color: isOnline
                                              ? Colors.greenAccent.withValues(
                                                  alpha: 0.4,
                                                )
                                              : Colors.redAccent.withValues(
                                                  alpha: 0.4,
                                                ),
                                          blurRadius: 6.0,
                                          spreadRadius: 2.0,
                                        ),
                                      ],
                                    ),
                                  ),

                                  const SizedBox(width: 10.0),
                                  Text(
                                    widget.device.name,
                                    maxLines: 1,
                                    overflow: TextOverflow.ellipsis,
                                    style: const TextStyle(
                                      fontSize: 17,
                                      fontWeight: FontWeight.bold,
                                      letterSpacing: -0.3,
                                    ),
                                  ),
                                ],
                              ),

                              const SizedBox(height: 2),
                              Text(
                                "${widget.device.ip}:${widget.device.port}",
                                style: TextStyle(
                                  fontSize: 11,
                                  color: theme.colorScheme.onSurfaceVariant
                                      .withValues(alpha: 0.6),
                                  fontFamily:
                                      'monospace', // Gives network IPs a nice technical structure
                                ),
                              ),
                            ],
                          ),
                        ],
                      ),
                    ),
                  );
                },
              );
            },
          ),
        );
      },
    );
  }
}
