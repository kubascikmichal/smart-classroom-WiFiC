# ESP32-S3-DeviceKitC-1 Demo

## Overview  

This project demonstrates how to use the ESP32-S3-DeviceKitC-1 to connect to Wi-Fi, generate random sensor values, and send them to a server using HTTP and UDP protocols. The project is divided into two branches to showcase different communication methods:

- `wifi_http_requests`: This branch contains code for sending sensor data to a server using HTTP POST requests.  

- `wifi_udp_requests`: This branch contains code for sending sensor data to a server using UDP.

- `Main Branch`: Contains the project overview and documentation. 

## Getting Started

```
# Clone the repo
git clone https://github.com/kubascikmichal/smart-classroom-WiFiC

# Navigate to the following repository 
cd smart-classroom-WiFiC

# Checkout to the
git checkout <branch_name>
Replace <repository_url> with the URL of your repository and <branch_name> with either http-requests or udp-requests.
```

Project Structure
http-requests branch: Contains the implementation for sending sensor data via HTTP POST requests.
udp-requests branch: Contains the implementation for sending sensor data via UDP.
Requirements
ESP32-S3-DeviceKitC-1
ESP-IDF development environment
cJSON library for JSON handling (for HTTP requests branch)
Setup Instructions
Configure Wi-Fi SSID and Password: Use idf.py menuconfig to set your Wi-Fi SSID and password.
Configure API Endpoint: Set the API endpoint and method if needed in the respective branch.
Build and Flash the Project:
sh
Copier le code
idf.py build
idf.py flash
Usage
HTTP Requests Branch
This branch demonstrates how to:

Connect to a Wi-Fi network.
Generate random sensor values.
Send the sensor values to a server using HTTP POST requests.
UDP Requests Branch
This branch demonstrates how to:

Connect to a Wi-Fi network.
Generate random sensor values.
Send the sensor values to a server using UDP.
Additional Information
Dependencies: Ensure all necessary dependencies are installed, such as the cJSON library for the HTTP requests branch.
Logging: Use the ESP_LOGI function for logging key events and data points for debugging and monitoring.
Customization: You can extend the example to include more sensors or other types of communication protocols.