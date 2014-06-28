lte-mac
=======

OMNeT++ implementation of the Long Term Evolution Advanced (LTE-A) MAC layer.
Tested with inetmanet and OMNeT++ v. 4.3.1.

The BasicLTE.ned model must be within a node model with:
- An application module called "cli" which generate Ethernet frames.
- A WiFi module called "wlan".
