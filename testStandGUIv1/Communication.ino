void getData() {

  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  char packet[255];
  int len = udp.read(packet, 255);
  if (len > 0) packet[len] = 0;

  String data = String(packet);

  int i1 = data.indexOf(',');
  int i2 = data.indexOf(',', i1 + 1);
  int i3 = data.indexOf(',', i2 + 1);

  if (i1 > 0 && i2 > 0 && i3 > 0) {

    current  = data.substring(0, i1).toFloat();
    voltage  = data.substring(i1 + 1, i2).toFloat();
    thrust   = data.substring(i2 + 1, i3).toFloat();
    throttle = data.substring(i3 + 1).toInt();
  }
}

void sendCommand(String cmd) {
  IPAddress broadcastIP(255,255,255,255);
  udp.beginPacket(broadcastIP, UDP_PORT);
  udp.print(cmd);
  udp.endPacket();
  Serial.print("Sent command: ");
  Serial.println(cmd);
}