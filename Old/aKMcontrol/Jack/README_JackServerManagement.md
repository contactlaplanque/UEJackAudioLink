# Jack Server Management

This document describes the new Jack server management functionality in the aKMcontrol project.

## Overview

The Jack audio system now supports manual server management instead of relying on Jack's auto-start feature. This allows for better control over Jack server parameters and ensures consistent audio configuration.

## New Features

### 1. Server Detection
- `CheckJackServerRunning()`: Checks if a Jack server is currently running
- Returns true if a server is found, false otherwise

### 2. Server Termination
- `KillJackServer()`: Terminates any running Jack server processes
- Uses `taskkill /f /im jackd.exe` on Windows
- Provides console feedback about the operation

### 3. Server Startup
- `StartJackServer()`: Starts Jack server with default parameters
- `StartJackServerWithParameters()`: Starts Jack server with custom parameters
- Default command: `C:/Program Files/JACK2/jackd.exe -S -X winmme -d portaudio -r 48000 -p 512`

### 4. Server Management
- `EnsureJackServerRunning()`: Complete server management workflow
  1. Checks if server is running
  2. Kills existing server if found
  3. Starts new server with custom parameters
  4. Verifies server is responding

## Configuration

The `AJackAudioInterface` class now includes configurable properties:

- **JackServerPath**: Path to jackd.exe (default: "C:/Program Files/JACK2/jackd.exe")
- **SampleRate**: Audio sample rate in Hz (default: 48000)
- **BufferSize**: Audio buffer size in frames (default: 512)
- **AudioDriver**: Audio driver to use (default: "portaudio")
- **bAutoStartServer**: Whether to automatically start server on BeginPlay (default: true)

## Usage

### Automatic Startup (Default)
The Jack server will automatically start with custom parameters when the `AJackAudioInterface` actor begins play.

### Manual Control
You can manually control the server using Blueprint functions:

1. **Check if server is running**:
   ```
   bool IsRunning = JackAudioInterface->CheckJackServerRunning();
   ```

2. **Kill existing server**:
   ```
   JackAudioInterface->KillJackServer();
   ```

3. **Start server with custom parameters**:
   ```
   bool Success = JackAudioInterface->StartJackServerWithParameters(
       "C:/Program Files/JACK2/jackd.exe",
       48000,  // Sample rate
       512,    // Buffer size
       "portaudio"  // Driver
   );
   ```

4. **Complete server management**:
   ```
   bool Success = JackAudioInterface->EnsureJackServerRunning();
   ```

## Console Output

The system provides detailed console output for debugging:

```
JackAudioInterface: Starting Jack audio system...
JackAudioInterface: Found existing Jack server, killing it to restart with custom parameters
JackClient: Attempting to kill existing Jack server...
JackClient: Successfully killed existing Jack server
JackClient: Starting Jack server with custom parameters - Sample Rate: 48000, Buffer Size: 512, Driver: portaudio
JackClient: Jack server process started successfully with custom parameters
JackClient: Jack server is now running and ready for connections
JackAudioInterface: Jack server started with custom parameters
JackAudioInterface: Jack server is ready, connecting client...
JackClient: Connected to Jack server (started by this client with custom parameters)
JackClient: Successfully connected to Jack server
JackClient: Sample rate: 48000, Buffer size: 512
JackAudioInterface: Successfully connected to Jack server
JackAudioInterface: Successfully registered 64 I/O ports
```

## Requirements

- Jack2 must be installed at the specified path (default: "C:/Program Files/JACK2/jackd.exe")
- The application must have permission to start and terminate processes
- Windows platform (uses Windows-specific process management)

## Troubleshooting

1. **Server not found**: Check that Jack2 is installed at the correct path
2. **Permission denied**: Run the application with appropriate permissions
3. **Server not responding**: Check that no other applications are using the audio device
4. **Port registration failed**: Ensure the server is fully started before connecting

## Blueprint Integration

 