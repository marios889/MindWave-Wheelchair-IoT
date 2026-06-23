class NetworkValidator {
  /// Validates standard IPv4 addresses
  static bool isValidIp(String ip) {
    final ipRegex = RegExp(
      r'^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$',
    );
    return ipRegex.hasMatch(ip);
  }

  /// Validates standard networking port bounds
  static bool isValidPort(String portStr) {
    final int? port = int.tryParse(portStr);
    if (port == null) return false;
    return port >= 1 && port <= 65535;
  }
}
