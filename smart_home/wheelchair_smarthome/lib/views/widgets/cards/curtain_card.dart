import 'package:flutter/material.dart';
import 'package:wheelchair_smarthome/data/models/smart_device.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';
import 'package:wheelchair_smarthome/views/widgets/dialogs/popupmenu_dialog.dart';

class CurtainCard extends StatefulWidget {
  const CurtainCard({super.key, required this.device});
  final SmartDevice device;
  @override
  State<CurtainCard> createState() => _CurtainCardState();
}

class _CurtainCardState extends State<CurtainCard> {
  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return InkWell(
      onLongPress: () => PopupmenuDialog.showPopupMenu(context, widget.device),
      child: ValueListenableBuilder(
        valueListenable: widget.device.isOnline,
        builder: (context, isOnline, child) {
          return ValueListenableBuilder(
            valueListenable: widget.device.isOn,
            builder: (context, powerState, child) {
              final Color cardBackground = !isOnline
                  ? theme.colorScheme.surfaceContainerHighest.withValues(
                      alpha: 0.4,
                    ) // Flat neutral gray background
                  : powerState
                  ? theme.colorScheme.primary.withValues(alpha: 0.08)
                  : theme.colorScheme.surface;

              final Color cardBorderColor = !isOnline
                  ? theme.colorScheme.onSurface.withValues(alpha: 0.1)
                  : powerState
                  ? theme.colorScheme.primary.withValues(alpha: 0.4)
                  : theme.colorScheme.onSurface.withValues(alpha: 0.25);
              return AnimatedContainer(
                duration: const Duration(milliseconds: 200),
                padding: const EdgeInsets.all(24.0),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(20.0),
                  // Subtle color change if the device is turned on
                  color: cardBackground,
                  border: Border.all(color: cardBorderColor, width: 1.5),
                  boxShadow: [
                    if (powerState && isOnline)
                      BoxShadow(
                        color: theme.colorScheme.primary.withValues(alpha: 0.1),
                        blurRadius: 8,
                        offset: const Offset(0, 4),
                      ),
                  ],
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
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
                                    ) // Grayed circle out
                                  : powerState
                                  ? Colors.white.withValues(alpha: 0.9)
                                  : Colors.white.withValues(alpha: 0.35),

                              shape: BoxShape.circle,
                            ),
                            child: SizedBox(
                              width: 55.0,
                              height: 55.0,
                              child: Opacity(
                                opacity: isOnline ? 1.0 : 0.4,
                                child: Image.asset(
                                  "assets/images/smart-curtain.png",
                                  fit: BoxFit.cover,
                                ),
                              ),
                            ),
                          ),
                          ElevatedButton(
                            style: ElevatedButton.styleFrom(
                              backgroundColor: !isOnline
                                  ? theme.colorScheme.surfaceDim
                                  : null,
                              foregroundColor: !isOnline
                                  ? theme.colorScheme.onSurfaceVariant
                                  : null,
                            ),
                            onPressed: isOnline
                                ? () async {
                                    for (int i = 0; i < 3; i++) {
                                      UdpService.sendUDP(
                                        "S",
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
                                  }
                                : null,
                            child: Text(
                              "Stop",
                              style: TextStyle(fontWeight: FontWeight.bold),
                            ),
                          ),
                          // ElevatedButton(
                          //   style: ElevatedButton.styleFrom(
                          //     backgroundColor: !isOnline
                          //         ? theme.colorScheme.surfaceDim
                          //         : null,
                          //     foregroundColor: !isOnline
                          //         ? theme.colorScheme.onSurfaceVariant
                          //         : null,
                          //   ),
                          //   onPressed: isOnline
                          //       ? () {
                          //           UdpService.sendUDP(
                          //             "R",
                          //             widget.device.ip,
                          //             int.parse(widget.device.port),
                          //           );
                          //         }
                          //       : null,
                          //   child: Text(
                          //     "Reset",
                          //     style: TextStyle(fontWeight: FontWeight.bold),
                          //   ),
                          // ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 10.0),
                    ValueListenableBuilder(
                      valueListenable: widget.device.sliderValue,
                      builder: (context, closeValue, child) {
                        return AbsorbPointer(
                          absorbing: !isOnline,
                          child: Column(
                            children: [
                              Text(
                                "Close Percentage",
                                style: TextStyle(
                                  fontSize: 16,
                                  fontWeight: FontWeight.bold,
                                  color: isOnline
                                      ? null
                                      : theme.colorScheme.onSurface.withValues(
                                          alpha: 0.38,
                                        ),
                                ),
                              ),
                              Text(
                                "${closeValue}",
                                style: TextStyle(
                                  fontSize: 16,
                                  fontWeight: FontWeight.bold,
                                  color: isOnline
                                      ? theme.colorScheme.primary
                                      : theme.colorScheme.onSurface.withValues(
                                          alpha: 0.38,
                                        ),
                                ),
                              ),
                              const SizedBox(height: 10.0),
                              Row(
                                mainAxisAlignment: MainAxisAlignment.center,
                                children: [
                                  const SizedBox(width: 15.0),
                                  ElevatedButton(
                                    onPressed: isOnline
                                        ? () async {
                                            for (int i = 0; i < 3; i++) {
                                              UdpService.sendUDP(
                                                "C100",
                                                widget.device.ip,
                                                int.parse(widget.device.port),
                                                useWatchdog: true,
                                                timeoutDuration: const Duration(
                                                  seconds: 10,
                                                ),
                                              );
                                              await Future.delayed(
                                                const Duration(
                                                  milliseconds: 100,
                                                ),
                                              );
                                            }

                                            DevicesState.changeOpenPercentage(
                                              widget.device.id,
                                              100.0,
                                            );
                                            DevicesState.toggleDeviceStatus(
                                              widget.device.id,
                                              true,
                                            );
                                          }
                                        : null,
                                    child: Text(
                                      "Close",
                                      style: TextStyle(
                                        fontWeight: FontWeight.bold,
                                      ),
                                    ),
                                  ),
                                  const SizedBox(width: 10.0),
                                  ElevatedButton(
                                    onPressed: isOnline
                                        ? () async {
                                            for (int i = 0; i < 3; i++) {
                                              UdpService.sendUDP(
                                                "O100",
                                                widget.device.ip,
                                                int.parse(widget.device.port),
                                                useWatchdog: true,
                                                timeoutDuration: const Duration(
                                                  seconds: 10,
                                                ),
                                              );
                                              await Future.delayed(
                                                const Duration(
                                                  milliseconds: 100,
                                                ),
                                              );
                                            }

                                            DevicesState.changeOpenPercentage(
                                              widget.device.id,
                                              0.0,
                                            );
                                            DevicesState.toggleDeviceStatus(
                                              widget.device.id,
                                              false,
                                            );
                                          }
                                        : null,
                                    child: const Text(
                                      "Open",
                                      style: TextStyle(
                                        fontWeight: FontWeight.bold,
                                      ),
                                    ),
                                  ),
                                ],
                              ),
                              Slider(
                                max: 100.0,
                                min: 0.0,
                                divisions: 10,
                                value: closeValue,
                                label: "${closeValue.toInt()}",
                                activeColor: isOnline
                                    ? null
                                    : theme.colorScheme.outlineVariant,
                                inactiveColor: isOnline
                                    ? null
                                    : theme.colorScheme.surfaceContainerHighest,
                                onChanged: isOnline
                                    ? (newValue) {
                                        DevicesState.changeOpenPercentage(
                                          widget.device.id,
                                          newValue,
                                        );
                                        if (newValue == 100.0) {
                                          DevicesState.toggleDeviceStatus(
                                            widget.device.id,
                                            true,
                                          );
                                        }
                                        if (newValue == 10.0) {
                                          DevicesState.toggleDeviceStatus(
                                            widget.device.id,
                                            false,
                                          );
                                        }
                                      }
                                    : null,
                              ),
                              const SizedBox(height: 10.0),
                              Row(
                                mainAxisAlignment: MainAxisAlignment.center,
                                children: [
                                  FilledButton(
                                    style: FilledButton.styleFrom(
                                      backgroundColor: !isOnline
                                          ? theme.colorScheme.surfaceDim
                                          : null,
                                      foregroundColor: !isOnline
                                          ? theme.colorScheme.onSurfaceVariant
                                          : null,
                                    ),
                                    onPressed: () async {
                                      for (int i = 0; i < 3; i++) {
                                        UdpService.sendUDP(
                                          closeValue != 0.0
                                              ? "C${closeValue.toInt()}"
                                              : "O100",
                                          widget.device.ip,
                                          int.parse(widget.device.port),
                                          useWatchdog: true,
                                          timeoutDuration: const Duration(
                                            seconds: 10,
                                          ),
                                        );
                                        await Future.delayed(
                                          const Duration(milliseconds: 100),
                                        );
                                      }
                                    },
                                    child: Text("Send Current Value"),
                                  ),
                                ],
                              ),
                            ],
                          ),
                        );
                      },
                    ),

                    const SizedBox(height: 10.0),
                    Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Row(
                          mainAxisAlignment: MainAxisAlignment.start,
                          children: [
                            ValueListenableBuilder(
                              valueListenable: widget.device.isOnline,
                              builder: (context, isOnline, child) {
                                return Container(
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
                                );
                              },
                            ),

                            const SizedBox(width: 9.0),
                            Text(
                              widget.device.name,
                              style: TextStyle(
                                color: isOnline
                                    ? theme.colorScheme.onSurface
                                    : theme.colorScheme.onSurface.withValues(
                                        alpha: 0.38,
                                      ),
                                fontSize: 16,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        Text(
                          "${widget.device.ip}:${widget.device.port}",
                          style: TextStyle(
                            color: isOnline
                                ? theme.colorScheme.onSurfaceVariant.withValues(
                                    alpha: 0.8,
                                  )
                                : theme.colorScheme.onSurface.withValues(
                                    alpha: 0.3,
                                  ),
                            fontSize: 14,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              );
            },
          );
        },
      ),
    );
  }
}
