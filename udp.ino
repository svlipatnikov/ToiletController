// функции UDP 

//=========================================================================================
// прием пакетов по UDP
 

bool Receive_UDP(void) {
  int packetSize = Udp.parsePacket();  
  if (packetSize)  {    
    int len = Udp.read(Buffer, UDP_TX_PACKET_MAX_SIZE); 
    if ((len == 2) && (Buffer[0] == 'w') && (Buffer[1] == '0')) { 
      Water_alarm_time = millis();
      Alarm_flag = true;           
    }
  }

  // если нет приема по UDP - сбрасываем флаги
  if ((long)millis() - Water_alarm_time > WATER_BLOCK_TIME) Alarm_flag = false;
}
