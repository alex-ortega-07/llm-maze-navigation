#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <time.h>
#include <vector>

#include "Cell.hpp"
#include "StdinReader.hpp"

using Grid = std::vector<std::vector<Cell>>;

enum class Direction { Up, Down, Left, Right };

// Default maze size (in cells) and allowed range for the command-line options
const int DEFAULT_GRID_COLS = 20;
const int DEFAULT_GRID_ROWS = 10;
const int MIN_GRID_SIZE = 2;
const int MAX_GRID_SIZE = 50;

const int CELL_WIDTH = 40;
const int MARGIN = 20;

const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color CELL_VISITING = {50, 123, 168, 255};
// Semi-transparent (alpha 120) so the background shows through the markers
const SDL_Color CELL_START_WALL = {0, 255, 0, 120};
const SDL_Color CELL_END_WALL = {255, 0, 0, 120};
const SDL_Color PLAYER = {255, 220, 0, 255};
const SDL_Color BACKGROUND = {97, 52, 235, 255};
const SDL_Color SELECTED = {100, 100, 200, 255};

const int FPS = 60;
const int FRAME_DELAY = 1000 / FPS;

struct Options {
    int cols = DEFAULT_GRID_COLS;
    int rows = DEFAULT_GRID_ROWS;
    bool agent_mode = false;
    bool has_seed = false;
    unsigned int seed = 0;
    bool fast = false;
};

enum class ParseResult { Ok, Help, Error };

void print_usage(const char *program_path){
    // Show only the file name, not the full path
    const char *program = program_path;
    for(const char *c = program_path; *c != '\0'; ++c){
        if(*c == '/' || *c == '\\'){
            program = c + 1;
        }
    }

    std::cout << "Usage: " << program << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  -w, --width  <n>   Number of columns (" << MIN_GRID_SIZE << "-" << MAX_GRID_SIZE
              << ", default " << DEFAULT_GRID_COLS << ")\n"
              << "  -h, --height <n>   Number of rows    (" << MIN_GRID_SIZE << "-" << MAX_GRID_SIZE
              << ", default " << DEFAULT_GRID_ROWS << ")\n"
              << "      --agent        Agent mode: read actions from stdin and write\n"
              << "                     observations to stdout (JSON Lines); WASD is disabled\n"
              << "      --seed <n>     Random seed (0-" << UINT_MAX << "). The same seed and size\n"
              << "                     always produce the same maze, entrance and exit\n"
              << "      --fast         Generate the maze instantly (same maze as without it) and,\n"
              << "                     in agent mode, reply to each action without waiting for a frame\n"
              << "      --help         Show this help and exit\n";
}

// Converts text to an int inside [MIN_GRID_SIZE, MAX_GRID_SIZE]. Returns false if invalid.
bool parse_grid_size(const char *text, int &out){
    char *end = nullptr;
    errno = 0;
    long value = std::strtol(text, &end, 10);

    if(end == text || *end != '\0' || errno == ERANGE){
        return false;
    }

    if(value < MIN_GRID_SIZE || value > MAX_GRID_SIZE){
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

// Converts text to an unsigned int seed. Returns false if invalid.
bool parse_seed(const char *text, unsigned int &out){
    // strtoul would silently accept negative numbers by wrapping them around
    if(text[0] == '-'){
        return false;
    }

    char *end = nullptr;
    errno = 0;
    unsigned long value = std::strtoul(text, &end, 10);

    if(end == text || *end != '\0' || errno == ERANGE || value > UINT_MAX){
        return false;
    }

    out = static_cast<unsigned int>(value);
    return true;
}

ParseResult parse_args(int argc, char *argv[], Options &options){
    for(int i = 1; i < argc; ++i){
        const char *arg = argv[i];

        if(std::strcmp(arg, "--help") == 0){
            return ParseResult::Help;
        }

        if(std::strcmp(arg, "--agent") == 0){
            options.agent_mode = true;
            continue;
        }

        if(std::strcmp(arg, "--fast") == 0){
            options.fast = true;
            continue;
        }

        if(std::strcmp(arg, "--seed") == 0){
            if(i + 1 >= argc){
                std::cerr << "Missing value for " << arg << "\n";
                return ParseResult::Error;
            }

            const char *value = argv[++i];

            if(!parse_seed(value, options.seed)){
                std::cerr << "Invalid value for " << arg << ": '" << value << "' (must be a number between 0 and "
                          << UINT_MAX << ")\n";
                return ParseResult::Error;
            }

            options.has_seed = true;
            continue;
        }

        bool is_width = std::strcmp(arg, "-w") == 0 || std::strcmp(arg, "--width") == 0;
        bool is_height = std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--height") == 0;

        if(!is_width && !is_height){
            std::cerr << "Unknown option: " << arg << "\n";
            return ParseResult::Error;
        }

        if(i + 1 >= argc){
            std::cerr << "Missing value for " << arg << "\n";
            return ParseResult::Error;
        }

        const char *value = argv[++i];
        int &target = is_width ? options.cols : options.rows;

        if(!parse_grid_size(value, target)){
            std::cerr << "Invalid value for " << arg << ": '" << value << "' (must be a number between "
                      << MIN_GRID_SIZE << " and " << MAX_GRID_SIZE << ")\n";
            return ParseResult::Error;
        }
    }

    return ParseResult::Ok;
}

void quit_wall(Grid &grid, int index_y, int index_x, Direction wall){
    switch(wall){
        case Direction::Right:
            grid[index_y][index_x].wall.r = false;
            grid[index_y][index_x + 1].wall.l = false;
            break;

        case Direction::Left:
            grid[index_y][index_x].wall.l = false;
            grid[index_y][index_x - 1].wall.r = false;
            break;

        case Direction::Up:
            grid[index_y][index_x].wall.u = false;
            grid[index_y - 1][index_x].wall.d = false;
            break;

        case Direction::Down:
            grid[index_y][index_x].wall.d = false;
            grid[index_y + 1][index_x].wall.u = false;
            break;
    }
}

// A cell on the edge of the grid and the outer wall that can be opened
struct BorderOpening {
    int y, x;
    Direction side;
};

// Lists every outer wall that can be opened (corner cells appear twice, once per side)
std::vector<BorderOpening> border_openings(const Grid &grid){
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    std::vector<BorderOpening> openings;

    for(int x = 0; x < cols; ++x){
        openings.push_back({0, x, Direction::Up});
        openings.push_back({rows - 1, x, Direction::Down});
    }

    for(int y = 0; y < rows; ++y){
        openings.push_back({y, 0, Direction::Left});
        openings.push_back({y, cols - 1, Direction::Right});
    }

    return openings;
}

// Removes only the outer wall of a border cell (quit_wall would also touch a neighbour outside the grid)
void open_border_wall(Grid &grid, const BorderOpening &opening){
    Cell &cell = grid[opening.y][opening.x];

    switch(opening.side){
        case Direction::Up:    cell.wall.u = false; break;
        case Direction::Down:  cell.wall.d = false; break;
        case Direction::Left:  cell.wall.l = false; break;
        case Direction::Right: cell.wall.r = false; break;
    }
}

// Picks two different border cells at random and opens their outer walls
void pick_entrance_and_exit(Grid &grid, BorderOpening &entrance, BorderOpening &exit){
    std::vector<BorderOpening> openings = border_openings(grid);

    entrance = openings[rand() % openings.size()];

    do{
        exit = openings[rand() % openings.size()];
    } while(exit.y == entrance.y && exit.x == entrance.x);

    open_border_wall(grid, entrance);
    open_border_wall(grid, exit);
}

// Returns true if the cell at (y, x) has no wall in that direction and the target cell is inside the grid
bool can_move(const Grid &grid, int y, int x, Direction dir){
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());
    const Cell &cell = grid[y][x];

    switch(dir){
        case Direction::Up:    return !cell.wall.u && y - 1 >= 0;
        case Direction::Down:  return !cell.wall.d && y + 1 < rows;
        case Direction::Left:  return !cell.wall.l && x - 1 >= 0;
        case Direction::Right: return !cell.wall.r && x + 1 < cols;
    }

    return false;
}

// Maps W/A/S/D (by physical key position) to a direction. Returns false for any other key.
bool direction_from_key(SDL_Scancode key, Direction &dir){
    switch(key){
        case SDL_SCANCODE_W: dir = Direction::Up;    return true;
        case SDL_SCANCODE_S: dir = Direction::Down;  return true;
        case SDL_SCANCODE_A: dir = Direction::Left;  return true;
        case SDL_SCANCODE_D: dir = Direction::Right; return true;
        default:             return false;
    }
}

void move(int &y, int &x, Direction dir){
    switch(dir){
        case Direction::Up:    --y; break;
        case Direction::Down:  ++y; break;
        case Direction::Left:  --x; break;
        case Direction::Right: ++x; break;
    }
}

// ---------------------------------------------------------------------------
// Agent mode: communication with the supervisor (see DECISIONS.md, decision 11)
// ---------------------------------------------------------------------------

const char *direction_name(Direction dir){
    switch(dir){
        case Direction::Up:    return "up";
        case Direction::Down:  return "down";
        case Direction::Left:  return "left";
        case Direction::Right: return "right";
    }

    return "";
}

bool direction_from_name(const std::string &name, Direction &dir){
    if(name == "up")    { dir = Direction::Up;    return true; }
    if(name == "down")  { dir = Direction::Down;  return true; }
    if(name == "left")  { dir = Direction::Left;  return true; }
    if(name == "right") { dir = Direction::Right; return true; }
    return false;
}

// Reads the string value of `key` from a flat JSON object such as {"type": "action", "action": "up"}.
// Minimal on purpose: it does not support escaped quotes or nested objects, which the protocol does not use.
bool json_string_field(const std::string &json, const std::string &key, std::string &value){
    const std::string quoted_key = "\"" + key + "\"";
    size_t pos = json.find(quoted_key);

    while(pos != std::string::npos){
        size_t i = pos + quoted_key.size();

        while(i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;

        // The same text can also appear as a value ("type": "action"), so only accept it if a ':' follows
        if(i < json.size() && json[i] == ':'){
            ++i;
            while(i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;

            if(i >= json.size() || json[i] != '"'){
                return false;
            }

            size_t end = json.find('"', i + 1);
            if(end == std::string::npos){
                return false;
            }

            value = json.substr(i + 1, end - i - 1);
            return true;
        }

        pos = json.find(quoted_key, pos + 1);
    }

    return false;
}

// Returns true and sets `dir` if the line is a valid {"type": "action", "action": "<direction>"} message
bool parse_action_message(const std::string &line, Direction &dir){
    std::string type, action;

    return json_string_field(line, "type", type) && type == "action"
        && json_string_field(line, "action", action) && direction_from_name(action, dir);
}

// Writes one observation message to stdout. std::endl flushes it so the supervisor receives it immediately.
void send_observation(const Grid &grid, int y, int x, int step, bool done, bool last_action_valid){
    std::string valid_actions;
    const Direction all_directions[] = {Direction::Up, Direction::Down, Direction::Left, Direction::Right};

    for(Direction dir : all_directions){
        if(can_move(grid, y, x, dir)){
            if(!valid_actions.empty()) valid_actions += ", ";
            valid_actions += "\"" + std::string(direction_name(dir)) + "\"";
        }
    }

    std::cout << "{\"type\": \"observation\""
              << ", \"position\": [" << y << ", " << x << "]"
              << ", \"valid_actions\": [" << valid_actions << "]"
              << ", \"step\": " << step
              << ", \"done\": " << (done ? "true" : "false")
              << ", \"last_action_valid\": " << (last_action_valid ? "true" : "false")
              << "}" << std::endl;
}

// Draws a filled square inside the cell at (y, x), leaving `inset` pixels on each side
void draw_marker(SDL_Renderer *renderer, int y, int x, SDL_Color color, int inset = 5){
    // Blending is needed for the alpha channel to have any effect
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect marker {MARGIN + CELL_WIDTH * x + inset, MARGIN + CELL_WIDTH * y + inset,
                     CELL_WIDTH - 2 * inset, CELL_WIDTH - 2 * inset};
    SDL_RenderFillRect(renderer, &marker);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void draw_visiting_cell(SDL_Renderer *renderer, const Grid &grid, SDL_Color color){
    for(size_t i = 0; i < grid.size(); ++i){
        for(size_t k = 0; k < grid[i].size(); ++k){
            if(grid[i][k].visiting){
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_Rect visiting {MARGIN + CELL_WIDTH * static_cast<int>(k) + 1,
                                   MARGIN + CELL_WIDTH * static_cast<int>(i) + 1,
                                   CELL_WIDTH - 1, CELL_WIDTH - 1};
                SDL_RenderFillRect(renderer, &visiting);
            }
        }
    }
}

bool all_visited(const Grid &grid){
    for(const auto &row : grid){
        for(const auto &cell : row){
            if(!cell.visited){
                return false;
            }
        }
    }

    return true;
}

// Marks a neighbour as a frontier cell if it exists and has not been visited yet
void mark_visiting(Grid &grid, int neighbour_y, int neighbour_x, int parent_y, int parent_x){
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    if(neighbour_y < 0 || neighbour_y >= rows || neighbour_x < 0 || neighbour_x >= cols){
        return;
    }

    Cell &neighbour = grid[neighbour_y][neighbour_x];

    if(!neighbour.visited){
        neighbour.visiting = true;
        neighbour.parent_cell.y = parent_y;
        neighbour.parent_cell.x = parent_x;
    }
}

void add_visiting(Grid &grid, int cell_y, int cell_x){
    mark_visiting(grid, cell_y + 1, cell_x, cell_y, cell_x);
    mark_visiting(grid, cell_y - 1, cell_x, cell_y, cell_x);
    mark_visiting(grid, cell_y, cell_x + 1, cell_y, cell_x);
    mark_visiting(grid, cell_y, cell_x - 1, cell_y, cell_x);
}

// One step of the randomized Prim's algorithm: picks a random frontier cell, connects it to the
// cell it was reached from and adds its neighbours to the frontier. The normal mode runs one step
// per frame (animated); --fast runs them all at once. Both make the same rand() calls in the same
// order, so a seed gives the same maze in both modes.
void generation_step(Grid &grid, int &cell_y, int &cell_x){
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    std::vector<std::vector<int>> visiting_list;
    for(int i = 0; i < rows; ++i){
        for(int k = 0; k < cols; ++k){
            if(grid[i][k].visiting && !grid[i][k].visited){
                visiting_list.push_back({i, k});
            }
        }
    }

    int num = rand() % visiting_list.size();

    grid[visiting_list[num][0]][visiting_list[num][1]].visited = true;
    grid[visiting_list[num][0]][visiting_list[num][1]].visiting = false;

    cell_y = grid[visiting_list[num][0]][visiting_list[num][1]].parent_cell.y;
    cell_x = grid[visiting_list[num][0]][visiting_list[num][1]].parent_cell.x;

    if(visiting_list[num][0] - cell_y == -1){
        quit_wall(grid, cell_y, cell_x, Direction::Up);
    }

    else if(visiting_list[num][0] - cell_y == 1){
        quit_wall(grid, cell_y, cell_x, Direction::Down);
    }

    else if(visiting_list[num][1] - cell_x == -1){
        quit_wall(grid, cell_y, cell_x, Direction::Left);
    }

    else if(visiting_list[num][1] - cell_x == 1){
        quit_wall(grid, cell_y, cell_x, Direction::Right);
    }

    cell_y = visiting_list[num][0];
    cell_x = visiting_list[num][1];

    add_visiting(grid, cell_y, cell_x);
}

int main(int argc, char *argv[]){
    Options options;

    switch(parse_args(argc, argv, options)){
        case ParseResult::Help:
            print_usage(argv[0]);
            return 0;

        case ParseResult::Error:
            print_usage(argv[0]);
            return 1;

        case ParseResult::Ok:
            break;
    }

    const int GRID_COLS = options.cols;
    const int GRID_ROWS = options.rows;

    const int SCREEN_WIDTH = CELL_WIDTH * GRID_COLS + MARGIN * 2;
    const int SCREEN_HEIGHT = CELL_WIDTH * GRID_ROWS + MARGIN * 2;

    // Logs go to stderr: in agent mode stdout is reserved for JSON messages
    if(SDL_Init(SDL_INIT_VIDEO) < 0){
        std::cerr << "SDL could not be initialized. SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Maze generation algorithm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    if(win == NULL){
        std::cerr << "Window could not be created. SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    if(TTF_Init()){
        std::cerr << "TTF could not be initialized. TTF_Error" << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_SetWindowAlwaysOnTop(win, SDL_TRUE);
    SDL_SetWindowBordered(win, SDL_FALSE);
    SDL_SetWindowOpacity(win, 0.8);
    SDL_ShowCursor(SDL_DISABLE);

    // Without --seed, use the current time and log it so the maze can be reproduced later
    unsigned int seed = options.has_seed ? options.seed : static_cast<unsigned int>(time(NULL));
    std::cerr << "Seed: " << seed << std::endl;
    srand(seed);
    int cell_x = rand() % GRID_COLS;
    int cell_y = rand() % GRID_ROWS;

    Grid grid(GRID_ROWS, std::vector<Cell>(GRID_COLS));

    for(int i = 0; i < GRID_ROWS; ++i){
        for(int k = 0; k < GRID_COLS; ++k){
            grid[i][k].posx = MARGIN + CELL_WIDTH * k;
            grid[i][k].posy = MARGIN + CELL_WIDTH * i;
            grid[i][k].width = grid[i][k].height = CELL_WIDTH;
            grid[i][k].color = WHITE;
        }
    }

    grid[cell_y][cell_x].visited = true;

    const Uint8 *keyboard_state = SDL_GetKeyboardState(NULL);

    SDL_Event event;

    Uint32 frame_start;
    int frame_time;

    SDL_Point mouse;

    bool maze_finished = false;
    BorderOpening entrance, maze_exit;

    int player_y = 0, player_x = 0;
    bool reached_exit = false;

    // Agent mode state
    int step = 0;
    StdinReader stdin_reader;

    if(options.agent_mode){
        stdin_reader.start();
    }

    add_visiting(grid, cell_y, cell_x);

    // --fast: build the whole maze now instead of one step per frame
    if(options.fast){
        while(!all_visited(grid)){
            generation_step(grid, cell_y, cell_x);
        }
    }

    // In fast agent mode, actions are answered as soon as they arrive and the window is redrawn at most FPS times per second
    const bool fast_agent = options.fast && options.agent_mode;
    Uint32 last_render = 0;

    bool running = true;
    while(running){
        frame_start = SDL_GetTicks();

        SDL_GetMouseState(&mouse.x, &mouse.y);

        bool generating = !all_visited(grid);

        if(generating){
            generation_step(grid, cell_y, cell_x);
        }

        else if(!maze_finished){
            pick_entrance_and_exit(grid, entrance, maze_exit);
            maze_finished = true;

            player_y = entrance.y;
            player_x = entrance.x;

            if(options.agent_mode){
                send_observation(grid, player_y, player_x, step, false, true);
            }
        }

        // Agent mode: apply every action received since the last frame and reply with an observation
        if(options.agent_mode && maze_finished && !reached_exit){
            std::string line;

            while(!reached_exit && stdin_reader.pop(line)){
                Direction dir;
                // Invalid actions (blocked by a wall or malformed) do not move the player but still count as a step
                bool valid = parse_action_message(line, dir) && can_move(grid, player_y, player_x, dir);

                if(valid){
                    move(player_y, player_x, dir);
                }

                ++step;

                if(player_y == maze_exit.y && player_x == maze_exit.x){
                    reached_exit = true;
                    std::cerr << "You reached the exit!" << std::endl;
                }

                send_observation(grid, player_y, player_x, step, reached_exit, valid);
            }
        }

        // Agent mode: stop when the supervisor closes the connection
        if(options.agent_mode && stdin_reader.finished()){
            running = false;
        }

        if(keyboard_state[SDL_SCANCODE_ESCAPE]){
            running = false;
        }

        // Handle every pending event so no key press is lost
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_QUIT){
                running = false;
            }

            // WASD only in manual mode; in agent mode the moves come from the supervisor
            else if(event.type == SDL_KEYDOWN && !options.agent_mode && maze_finished && !reached_exit){
                Direction dir;

                if(direction_from_key(event.key.keysym.scancode, dir) && can_move(grid, player_y, player_x, dir)){
                    move(player_y, player_x, dir);

                    if(player_y == maze_exit.y && player_x == maze_exit.x){
                        reached_exit = true;
                        std::cerr << "You reached the exit!" << std::endl;
                    }
                }
            }
        }

        // Draw the window (every iteration normally, at most FPS times per second in fast agent mode)
        Uint32 now = SDL_GetTicks();

        if(!fast_agent || now - last_render >= static_cast<Uint32>(FRAME_DELAY)){
            last_render = now;

            SDL_SetRenderDrawColor(renderer, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, BACKGROUND.a);
            SDL_RenderClear(renderer);

            if(generating){
                draw_visiting_cell(renderer, grid, CELL_VISITING);
            }

            SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);

            for(int i = 0; i < GRID_ROWS; ++i){
                for(int k = 0; k < GRID_COLS; ++k){
                    grid[i][k].create(renderer);
                }
            }

            if(maze_finished){
                draw_marker(renderer, entrance.y, entrance.x, CELL_START_WALL);
                draw_marker(renderer, maze_exit.y, maze_exit.x, CELL_END_WALL);
                draw_marker(renderer, player_y, player_x, PLAYER, 12);
            }

            SDL_RenderPresent(renderer);
        }

        if(fast_agent){
            // Sleep until the next action arrives or it is time to redraw / handle window events
            Uint32 since_render = SDL_GetTicks() - last_render;
            int timeout = since_render >= static_cast<Uint32>(FRAME_DELAY) ? 0 : FRAME_DELAY - static_cast<int>(since_render);
            stdin_reader.wait_for_line(timeout);
        }

        else{
            frame_time = SDL_GetTicks() - frame_start;
            if(FRAME_DELAY > frame_time){
                SDL_Delay(FRAME_DELAY - frame_time);
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return 0;
}
