"""
ESP32 Vibration Monitor - Serial to CSV Logger
Captures data from ESP32 and saves to CSV file with timestamp
"""

import serial
import csv
from datetime import datetime
import os
import sys

# ==================== CONFIGURATION ====================
# Change this to your ESP32's COM port
# Windows: 'COM3', 'COM4', etc.
# Mac/Linux: '/dev/ttyUSB0', '/dev/cu.usbserial-*', etc.
SERIAL_PORT = '/dev/cu.usbmodem101'  # <--- CHANGE THIS
BAUD_RATE = 500000
TIMEOUT = 1

# CSV file settings
OUTPUT_FOLDER = 'vibration_logs'

# DEBUG MODE - Set to True to see what's being filtered
DEBUG_MODE = True
# =======================================================

def find_serial_port():
    """Helper function to list available serial ports"""
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        print("\n=== Available Serial Ports ===")
        for i, port in enumerate(ports, 1):
            print(f"{i}. {port.device} - {port.description}")
        return [port.device for port in ports]
    except:
        return []

def create_csv_filename():
    """Create filename with current date and time"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"vibration_log_{timestamp}.csv"
    
    # Create output folder if it doesn't exist
    if not os.path.exists(OUTPUT_FOLDER):
        os.makedirs(OUTPUT_FOLDER)
        print(f"✓ Created folder: {OUTPUT_FOLDER}")
    
    filepath = os.path.join(OUTPUT_FOLDER, filename)
    return filepath

def is_data_line(line):
    """Check if line is a CSV data line"""
    # Must contain commas
    if ',' not in line:
        return False
    
    # Split and check format
    parts = [p.strip() for p in line.split(',')]
    if len(parts) < 5:
        return False
    
    # First part should be a number (time in seconds)
    try:
        float(parts[0])
        return True
    except ValueError:
        return False

def main():
    print("=" * 60)
    print("ESP32 VIBRATION MONITOR - CSV LOGGER")
    print("=" * 60)
    
    # Check if port exists
    available_ports = find_serial_port()
    
    if not available_ports:
        print("\n⚠ No serial ports found!")
        print("Please connect your ESP32 and try again.")
        input("\nPress Enter to exit...")
        return
    
    # Try to connect to serial port
    try:
        print(f"\n📡 Connecting to {SERIAL_PORT} at {BAUD_RATE} baud...")
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT)
        print("✓ Connected successfully!")
        
        # Wait for serial port to stabilize
        import time
        time.sleep(2)
        
        # Flush any existing data
        ser.reset_input_buffer()
        
    except serial.SerialException as e:
        print(f"\n❌ Error: Could not open {SERIAL_PORT}")
        print(f"Details: {e}")
        print("\nAvailable ports:")
        for port in available_ports:
            print(f"  - {port}")
        print(f"\nUpdate SERIAL_PORT in the script to match your ESP32 port")
        input("\nPress Enter to exit...")
        return
    
    # Create CSV file
    csv_filename = create_csv_filename()
    print(f"\n📝 Logging to: {csv_filename}")
    print("\n" + "=" * 60)
    print("STATUS: Waiting for data from ESP32...")
    print("=" * 60)
    print("Press Ctrl+C to stop logging\n")
    
    csv_file = open(csv_filename, 'w', newline='', encoding='utf-8')
    csv_writer = csv.writer(csv_file)
    
    # Write header with real timestamp
    csv_writer.writerow(['Real_Timestamp', 'Device_Time(s)', 'Frequency(Hz)', 
                         'Magnitude', 'Anomaly', 'Alert'])
    csv_file.flush()
    
    line_count = 0
    header_found = False
    
    try:
        while True:
            try:
                # Read line from serial
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if not line:
                    continue
                
                # Print to console for monitoring
                print(line)
                
                # Check if this is the CSV header from ESP32
                if 'Time(s)' in line and 'Frequency' in line and 'Magnitude' in line:
                    header_found = True
                    print("\n✓✓✓ CSV HEADER DETECTED - READY TO LOG DATA ✓✓✓\n")
                    continue
                
                # Skip setup messages
                if any(keyword in line for keyword in [
                    '===', '---', 'ADXL345', 'Initialized', 
                    'Alert Settings', 'GPIO', 'Threshold:', 'Duration:',
                    'Vibration Monitor', 'CSV Data Log'
                ]):
                    continue
                
                # Skip status messages (but keep them visible in console)
                if '>>>' in line:
                    continue
                
                # Try to parse as data line (auto-detect if no header seen yet)
                is_valid = is_data_line(line)
                
                if is_valid:
                    # Auto-enable logging if we detect valid data (even without header)
                    if not header_found:
                        header_found = True
                        print("\n✓✓✓ DATA DETECTED - AUTO-STARTING LOG ✓✓✓\n")
                    
                    if DEBUG_MODE:
                        print(f"  [DEBUG] ✓ LOGGING THIS LINE")
                    
                    # Add real timestamp
                    real_timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
                    
                    # Split the CSV line
                    data_parts = [p.strip() for p in line.split(',')]
                    
                    # Write to CSV
                    csv_writer.writerow([real_timestamp] + data_parts)
                    csv_file.flush()  # Save immediately
                    
                    line_count += 1
                    
                    if line_count % 5 == 0:
                        print(f"\n  ✓✓✓ {line_count} data points logged ✓✓✓\n")
                elif DEBUG_MODE and ',' in line:
                    print(f"  [DEBUG] ✗ Skipping (not valid data format)")
                
            except UnicodeDecodeError:
                continue
            except Exception as e:
                print(f"  [ERROR] Problem with line: {e}")
                continue
                
    except KeyboardInterrupt:
        print("\n\n" + "=" * 60)
        print("📊 LOGGING STOPPED BY USER")
        print("=" * 60)
        print(f"Total data points logged: {line_count}")
        print(f"File saved: {csv_filename}")
        print("=" * 60)
    
    finally:
        # Clean up
        csv_file.close()
        ser.close()
        print("\n✓ Serial port closed")
        print("✓ CSV file saved successfully")
        input("\nPress Enter to exit...")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        input("\nPress Enter to exit...")