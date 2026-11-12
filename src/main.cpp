#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <time.h>
#include <vector>

#include "Cell.hpp"

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
const SDL_Color BACKGROUND = {97, 52, 235, 255};
const SDL_Color SELECTED = {100, 100, 200, 255};

const int FPS = 60;
const int FRAME_DELAY = 1000 / FPS;

struct Options {
    int cols = DEFAULT_GRID_COLS;
    int rows = DEFAULT_GRID_ROWS;
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

ParseResult parse_args(int argc, char *argv[], Options &options){
    for(int i = 1; i < argc; ++i){
        const char *arg = argv[i];

        if(std::strcmp(arg, "--help") == 0){
            return ParseResult::Help;
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

// Draws a filled square inside the cell at (y, x)
void draw_marker(SDL_Renderer *renderer, int y, int x, SDL_Color color){
    // Blending is needed for the alpha channel to have any effect
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect marker {MARGIN + CELL_WIDTH * x + 5, MARGIN + CELL_WIDTH * y + 5, CELL_WIDTH - 10, CELL_WIDTH - 10};
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

    if(SDL_Init(SDL_INIT_VIDEO) < 0){
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Maze generation algorithm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    if(win == NULL){
        std::cout << "Window could not be created. SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    if(TTF_Init()){
        std::cout << "TTF could not be initialized. TTF_Error" << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_SetWindowAlwaysOnTop(win, SDL_TRUE);
    SDL_SetWindowBordered(win, SDL_FALSE);
    SDL_SetWindowOpacity(win, 0.8);
    SDL_ShowCursor(SDL_DISABLE);

    srand(time(NULL));
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

    int num;

    SDL_Point mouse;

    bool maze_finished = false;
    BorderOpening entrance, maze_exit;

    std::vector<std::vector<int>> visiting_list;

    add_visiting(grid, cell_y, cell_x);

    bool running = true;
    while(running){
        frame_start = SDL_GetTicks();

        SDL_GetMouseState(&mouse.x, &mouse.y);

        SDL_SetRenderDrawColor(renderer, BACKGROUND.r, BACKGROUND.g, BACKGROUND.b, BACKGROUND.a);
        SDL_RenderClear(renderer);

        if(!all_visited(grid)){
            draw_visiting_cell(renderer, grid, CELL_VISITING);

            visiting_list.clear();
            for(int i = 0; i < GRID_ROWS; ++i){
                for(int k = 0; k < GRID_COLS; ++k){
                    if(grid[i][k].visiting && !grid[i][k].visited){
                        visiting_list.push_back({i, k});
                    }
                }
            }

            num = rand() % visiting_list.size();

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

        else if(!maze_finished){
            pick_entrance_and_exit(grid, entrance, maze_exit);
            maze_finished = true;
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
        }

        if(keyboard_state[SDL_SCANCODE_ESCAPE]){
            running = false;
        }

        SDL_PollEvent(&event);

        if(event.type == SDL_QUIT){
            running = false;
        }

        SDL_RenderPresent(renderer);

        frame_time = SDL_GetTicks() - frame_start;
        if(FRAME_DELAY > frame_time){
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return 0;
}
