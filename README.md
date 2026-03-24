# City Traffic Simulation

A multi-threaded bridge traffic simulation with three control modes: Police Officers, Traffic Lights, and Carnage.  
Built with C, POSIX threads, and SDL2 graphics.

## Features

- **Three control modes**:
    - **Police Mode**: Two independent police officers alternate turns, allowing up to `ki` vehicles per turn
    - **Traffic Lights Mode**: Independent traffic lights with configurable green times
    - **Carnage Mode**: One direction at a time

- **Ambulance Priority**: Ambulances can pass even on red lights or when police are inactive

- **10-second Timeout**: In Police mode, if one side cannot pass for 10 seconds, the other side gets a turn

- **Real-time GUI**: SDL2-based visualization showing:
    - Bridge with vehicles (ambulances marked with white cross)
    - Waiting queues at each end
    - Live statistics
    - Police/Traffic light status

- **Exponential Traffic Generation**: Vehicles arrive following exponential distribution

## Dependencies

| Library | Purpose |
|---------|---------|
| `libSDL2` | Graphics and window management |
| `libSDL2_ttf` | Text rendering for GUI |
| `pthread` | Multi-threading (POSIX threads) |
| `math.h` | Exponential distribution calculations |

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install gcc make libsdl2-dev libsdl2-ttf-dev
```

### Arch Linux

```bash
sudo pacman -S gcc make sdl2 sdl2_ttf
```

## Compilation

```bash
gcc -o bridge_simulation main.c bridge.c traffic_generator.c gui.c vehicle.c config.c -lpthread -lSDL2 -lSDL2_ttf -lm
```

## Usage

```bash
./bridge_simulation
```

## Configuration

Create a `config.txt` file:

```ini
# Bridge configuration
bridge_length=25
simulation_mode=police

# East → West traffic
east_arrival_mean=2.0
east_avg_speed=10
east_Ki=3
east_green_time=10
east_min_speed=8
east_max_speed=12
east_ambulance_percentage=20
east_speed_variation=2

# West → East traffic
west_arrival_mean=2.0
west_avg_speed=10
west_Ki=3
west_green_time=10
west_min_speed=8
west_max_speed=12
west_ambulance_percentage=20
west_speed_variation=2
```

## Simulation Modes

| Mode | Value | Description |
|------|-------|-------------|
| Police | `police` | Two independent police officers control traffic |
| Traffic Lights | `tlights` | Independent traffic lights with timing |
| Carnage | `carnage` | One direction at a time |

## Project Structure

```
bridge-simulation/
├── Makefile
├── config.txt
├── main.c
├── bridge.c
├── bridge.h
├── vehicle.c
├── vehicle.h
├── traffic_generator.c
├── traffic_generator.h
├── gui.c
├── gui.h
├── config.c
└── config.h
```

## Synchronization

The project uses POSIX synchronization primitives:

| Mechanism | Usage |
|-----------|-------|
| `pthread_mutex_t` | Protect shared data (bridge segments, counters) |
| `pthread_cond_t` | Block waiting for conditions (green light, empty bridge) |
| `pthread_mutex_trylock` | Non-blocking segment acquisition |
| `usleep` | Short delays between cycles |

## License

MIT License

## Author

**Diego Zamora Bustos**
- GitHub: [@dizabu](https://github.com/dizabu)
- David Zamora Barrantes
