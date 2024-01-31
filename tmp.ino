void setval(const char* keyString, const byte val) {
  if (strcmp_P(keyString, PSTR("troz_sh")) == 0) {
    conf.troz_sh = val;
  } else if (strcmp_P(keyString, PSTR("tnag_sh")) == 0) {
    conf.tnag_sh = val;
  } else if (strcmp_P(keyString, PSTR("tpod_sh")) == 0) {
    conf.tpod_sh = val;
  } else if (strcmp_P(keyString, PSTR("tsh_st")) == 0) {
    conf.tsh_st = val;
  } else if (strcmp_P(keyString, PSTR("troz")) == 0) {
    conf.troz = val;
  } else if (strcmp_P(keyString, PSTR("fl_fix")) == 0) {
    conf.fl_fix = val;
  } else if (strcmp_P(keyString, PSTR("tfl")) == 0) {
    conf.tfl = val;
  } else if (strcmp_P(keyString, PSTR("vroz")) == 0) {
    conf.vroz = val;
  } else if (strcmp_P(keyString, PSTR("v_nag")) == 0) {
    conf.v_nag = val;
  } else if (strcmp_P(keyString, PSTR("v_pod")) == 0) {
    conf.v_pod = val;
  } else if (strcmp_P(keyString, PSTR("v_og")) == 0) {
    conf.v_og = val;
  } else if (strcmp_P(keyString, PSTR("temp")) == 0) {
    conf.temp = val;
  } else if (strcmp_P(keyString, PSTR("gister")) == 0) {
    conf.gister = val;
  } else if (strcmp_P(keyString, PSTR("t_vizh")) == 0) {
    conf.t_vizh = val;
  }
}



boolean sendstate(EthernetClient& ethClient, String request) {
  if (request.indexOf(F("getstate")) != -1 ){
    ethClient.println(F("HTTP/1.1 200 OK"));
    //ethClient.println(F("Content-Type: text/JSON"));
    ethClient.println();
    ethClient.print(F("{\"regim\":"));
    ethClient.print(opt.regim);
    ethClient.print(F(",\"shnekStart\":"));
    ethClient.print(shnekStart);
    ethClient.print(F(",\"lampaStart\":"));
    ethClient.print(lampaStart);
    ethClient.print(F(",\"vspeed\":"));
    ethClient.print(vspeed);
    ethClient.print(F(",\"flamePersent\":"));
    ethClient.print(flamePersent);
    ethClient.print(F(",\"temperVal\":"));
    ethClient.print(temperVal);
    ethClient.print("}");
    return false;
  } else {
    return true;
  }
}