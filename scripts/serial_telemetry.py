"""
VoxGate v2 Telemetry Logger
Listens to the ESP32-S3 serial port, prints colorized live output, 
and logs trigger events (confidence, latency, DSP time) to a CSV file.
"""

import serial
import serial.tools.list_ports
import time
import csv
import argparse
import re
import sys
from datetime import datetime

try:
    from colorama import init, Fore, Style
    init(autoreset=True)
except ImportError:
    print("Please install requirements: pip install -r requirements.txt")
    sys.exit(1)

def list_ports():
    """Returns a list of available serial ports."""
    ports = serial.tools.list_ports.comports()
    return [port.device for port in ports]

def main():
    parser = argparse.ArgumentParser(description="VoxGate v2 Serial Telemetry Logger")
    parser.add_argument("--port", type=str, help="Serial port (e.g., COM3 or /dev/ttyUSB0). If omitted, lists available ports.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--log", type=str, default="voxgate_telemetry.csv", help="Output CSV file (default: voxgate_telemetry.csv)")
    args = parser.parse_args()

    available_ports = list_ports()
    if not args.port:
        print(Fore.YELLOW + "No port specified. Available ports:")
        for p in available_ports:
            print(f"  - {p}")
        print("\nUsage example: python serial_telemetry.py --port COM3")
        sys.exit(1)


    conf_pattern = re.compile(r"Keyword Conf:\s+([\d.]+)%")
    time_pattern = re.compile(r"Latency:\s+(\d+)\s+ms\s+\|\s+DSP:\s+(\d+)\s+ms")

 
    try:
        with open(args.log, mode='a', newline='') as csv_file:
            writer = csv.writer(csv_file)
           
            if csv_file.tell() == 0:
                writer.writerow(["Timestamp", "Confidence (%)", "Inference Latency (ms)", "DSP Latency (ms)"])
    except Exception as e:
        print(Fore.RED + f"Failed to open log file: {e}")
        sys.exit(1)

  
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
        print(Fore.GREEN + f"[+] Connected to {args.port} at {args.baud} baud.")
        print(Fore.CYAN + f"[+] Logging data to {args.log}...")
        print("Waiting for VoxGate activations... (Press Ctrl+C to exit)\n")
    except Exception as e:
        print(Fore.RED + f"[-] Error opening serial port {args.port}: {e}")
        sys.exit(1)

    current_confidence = None

    try:
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                
                if "VOXGATE TRIGGERED" in line:
                    print(Fore.GREEN + Style.BRIGHT + line)
                elif "[-]" in line:
                    print(Fore.RED + line)
                elif "[+]" in line:
                    print(Fore.CYAN + line)
                elif line:
                    print(line)

               
                conf_match = conf_pattern.search(line)
                if conf_match:
                    current_confidence = float(conf_match.group(1))

               
                time_match = time_pattern.search(line)
                if time_match and current_confidence is not None:
                    latency = int(time_match.group(1))
                    dsp = int(time_match.group(2))
                    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

                    
                    with open(args.log, mode='a', newline='') as csv_file:
                        writer = csv.writer(csv_file)
                        writer.writerow([timestamp, current_confidence, latency, dsp])
                    
                    current_confidence = None 

            time.sleep(0.01)

    except KeyboardInterrupt:
        print(Fore.YELLOW + "\n[!] Telemetry stopped by user. Exiting...")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    main()