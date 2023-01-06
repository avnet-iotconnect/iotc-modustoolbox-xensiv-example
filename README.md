## iotc-modustoolbox-XENSIV-example

Avnet IoTConnect XENSIV&trade; project example for Infineon's ModusToolbox IDE and framework

## Requirements

- [ModusToolbox&trade; software](https://www.cypress.com/products/modustoolbox-software-environment) v3.0.0 or later (tested with v3.0.0)

## Supported Boards

The code has been developed and tested With:
- Rapid IoT connect developer kit (`CYSBSYSKIT-DEV-01`) with PAS CO2 sensor.

## Build Instructions

- Download, install and open [ModusToolbox&trade; software](https://www.cypress.com/products/modustoolbox-software-environment). On Windows, ensure that you **Run As Adminstrator** the installation package so that the neccessary drivers can be installed.
- Select a name for your workspace when prompted for a workspace name.
- Click the **New Application** link in the **Quick Panel** (or, use **File** > **New** > **ModusToolbox Application**). This launches the [Project Creator](https://www.cypress.com/ModusToolboxProjectCreator) tool.
- Pick a kit supported by the code example from the list shown in the **Project Creator - Choose Board Support Package (BSP)** dialog and click **Next**
- In the **Project Creator - Select Application** dialog, Click the checkbox of the project **Avnet IoTConnect Example** under **Wi-Fi** catergory and then click **Create**. 
- Modify Avnet_IoTConnect_Example/configs/app_config.h per your IoTConnect device and account info.
- At this point you should be able to build and run the application by using the options in the **Quick Panel** on bottom left of the screen.   
- You should see the application output in your terminal emulator.