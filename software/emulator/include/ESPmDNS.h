#pragma once
struct EmuMDNS {
  bool begin(const char *) { return true; }
  void addService(const char *, const char *, int) {}
};
extern EmuMDNS MDNS;
