import 'package:network_info_plus/network_info_plus.dart';
import 'package:wheelchair_smarthome/data/notifiers/devices_state.dart';
import 'package:wheelchair_smarthome/data/services/udp_service.dart';

class NetworkDiscoveryService {
  static Future<String?> getPrivateIp() async {
    final info = NetworkInfo();
    String? ip = await info.getWifiIP();
    return ip; // Fetches the local IP (e.g., 192.168.1.X)
  }

  static Future<void> initializeNetworkDiscovery(
    bool resetOffline,
    bool resetStates,
    int listeningPort,
    Map<String, dynamic> broadcastMessage,
    int broadcastPort,
    String? phoneIP,
  ) async {
    try {
      if (resetOffline) {
        DevicesState.resetAllToOffline();
      }
      if (resetStates) {
        DevicesState.resetAllStates();
      }

      await MultiUdpService.startListening(listeningPort);

      if (phoneIP != null) {
        UdpService.sendBroadcastMessage(broadcastPort, broadcastMessage);
      }
    } catch (e) {
      print("❌ Error starting page-level UDP tracking: $e");
    }
  }
}
