## **Wi-Fi Test Suite Linux Daemon for both AP and STA**

##Wi-Fi Test Suite Introduction
Wi-Fi Test Suite is a software platform originally developed by Wi-Fi Alliance, the global non-profit industry association that brings you Wi-Fi&reg;, to support certification program development and device certification. Non-proprietary components are provided under the ISC License and can be accessed at this open source project on GitHub. Wi-Fi Alliance members can access the full software package, including proprietary components, on the [Wi-Fi Alliance member site](https://www.wi-fi.org/members/certification-testing/sigma).

##Control Agents
Control agents are a proxy in which a CAPI control command[(CAPI specification)](http://www.wi-fi.org/file/wi-fi-test-suite-control-api-specification-v831) is converted for the device into the device’s native control interface. APs, DUTs, sniffers, and STAs may require control agents. The Linux control agent can be downloaded through the [open source repository](https://github.com/Wi-FiAlliance/Wi-FiTestSuite-Linux-DUT).

##Installation from sources
Refer to the [Install Guide](https://github.com/Wi-FiTestSuite/Wi-FiTestSuite-Linux-DUT/blob/master/Docs/INSTALL) for instructions on setting up a Linux DUT.

##License
Please refer to [LICENSE.txt](https://github.com/Wi-FiTestSuite/Wi-FiTestSuite-Linux-DUT/blob/master/LICENSE.txt).

## Issues and Contribution Guidelines
Please submit issues/ideas to [Wi-Fi Test Suite Google Group](https://groups.google.com/d/forum/wi-fitestsuite).
Both Wi-Fi Alliance members and non-members can contribute to the Wi-Fi Test Suite open source project. Please review the contribution agreement prior to submitting a pull request.
Please read more on contributions in [CONTRIBUTING.md](https://github.com/Wi-FiTestSuite/Wi-FiTestSuite-Linux-DUT/blob/master/CONTRIBUTING.md).

# This project is being developped by MUS_CSD_CSD14_SD17

The CA, DUT daemon now became a single daemon that support DBDC driver mode.
The usage

To develop this project under linux platform.

1. command out the cross compiler in Makefile.inc
2. make clean
3. make

to Run this daemon.
1. copy this daemon under /sbin/

2. Change dir to sbin -> cd /sbin/
  ./mtk_dut ap br0 9000

3. STA, TG have not implemented yet.

Created by #Yanfang Liu on 9/30/2019

