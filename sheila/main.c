// ============================================================
// SYSTEM INCLUDES
// ============================================================
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <sys/utsname.h>

// ============================================================
// MACROS/FEATURES
// ============================================================
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

//# define NDEBUG

//#define NALOGA1_REPL_1
//#define NALOGA1_REPL_2
//#define NALOGA1_REPL_3

// ============================================================
// CONSTANTS
// ============================================================
#define INPUT_BUFFER_SIZE 4096
#define CWD_MAXLEN 256
#define LINKREAD_MAXLEN 256
#define CPCAT_BUFF_MAXLEN 4096
#define SHELL_NAME_AND_INTIAL_PROMPT "mysh"
#define HISTORY_MAXLEN 16
#define PROCFS_DEFAULT_PATH "/proc"
#define PIDS_ARR_INITIAL_CAPACITY 8

#define INITIAL_DEBUG_LVL 0
#define DEEP_DEBUG_MODE 9 // extreme debug mode, dev only

// ============================================================
// TYPE DEFINITIONS
// ============================================================
typedef enum {
    STATE_START,
    STATE_IN_COMMENT,
    STATE_WORD,
    STATE_IN_DOUBLE_QUOTES,
} LexerState;

typedef enum {
    TOKEN_SYMBOL,
    TOKEN_REDIRECT_INPUT,
    TOKEN_REDIRECT_OUTPUT,
    TOKEN_RUN_IN_BACKGROUND,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    uint8_t*   str_val;
} Token;

typedef struct {
    uint8_t* input_line;
    Token*   tokens;
    int      count;
    int      capacity;
} TokenList;

typedef struct {

} CommandList;

typedef int32_t (*builtin_fn)(uint8_t** args);

typedef struct {
    const char* name;
    builtin_fn  fn;
} Builtin;

typedef struct {
    uint8_t** args;
    uint8_t*     redirect_in;
    uint8_t*     redirect_out;
    bool      run_in_bg;
} Command;

// ============================================================
// GLOBAL STATE
// ============================================================
bool IS_INTERACTIVE = false;
int32_t DEBUG_LVL = INITIAL_DEBUG_LVL;
int32_t LAST_EXIT_STATUS = 0;
uint8_t* PROMPT = (uint8_t*)SHELL_NAME_AND_INTIAL_PROMPT;
bool PROMPT_IS_HEAP = false;
uint8_t* HISTORY[HISTORY_MAXLEN] = {0};
uint32_t HISTORY_POSITION = 0;
uint8_t* PROCFS_PATH = (uint8_t*)PROCFS_DEFAULT_PATH;
bool PROCFS_PATH_IS_HEAP = false;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================
static inline void print_shell(void);
void print_command_args(uint8_t** args, int32_t start, const uint8_t* separator, bool new_line);
void emit_token(int32_t symbol_start, int32_t symbol_end, TokenType tt, const uint8_t* buffer, TokenList* token_list);
static inline bool is_word_char(uint8_t c);
void lexer(const uint8_t* buffer, TokenList* token_list);
void parser(const TokenList* token_list);
static uint8_t* expand_tilde(const uint8_t* str);
static int32_t comp_asc_int32_t(const void* a, const void* b);

int32_t argument_count(uint8_t** args);
void free_command(const Command* cmd);
Command build_command(const TokenList* token_list);
builtin_fn find_builtin(const uint8_t* command_str);

int32_t builtin_debug(uint8_t** args);
int32_t builtin_prompt(uint8_t** args);
int32_t builtin_status(uint8_t** args);
int32_t builtin_exit(uint8_t** args);
int32_t builtin_help(uint8_t** args);

int32_t builtin_print(uint8_t** args);
int32_t builtin_echo(uint8_t** args);
int32_t builtin_len(uint8_t** args);
int32_t builtin_sum(uint8_t** args);
int32_t builtin_calc(uint8_t** args);
int32_t builtin_basename(uint8_t** args);
int32_t builtin_dirname(uint8_t** args);

int32_t builtin_dirch(uint8_t** args);
int32_t builtin_dirwd(uint8_t** args);
int32_t builtin_dirmk(uint8_t** args);
int32_t builtin_dirrm(uint8_t** args);
int32_t builtin_dirls(uint8_t** args);

int32_t builtin_rename(uint8_t** args);
int32_t builtin_unlink(uint8_t** args);
int32_t builtin_remove(uint8_t** args);
int32_t builtin_linkhard(uint8_t** args);
int32_t builtin_linksoft(uint8_t** args);
int32_t builtin_linkread(uint8_t** args);
int32_t builtin_linklist(uint8_t** args);
int32_t builtin_cpcat(uint8_t** args);

int32_t builtin_pid(uint8_t** args);
int32_t builtin_ppid(uint8_t** args);
int32_t builtin_uid(uint8_t** args);
int32_t builtin_euid(uint8_t** args);
int32_t builtin_gid(uint8_t** args);
int32_t builtin_egid(uint8_t** args);
int32_t builtin_sysinfo(uint8_t** args);

int32_t builtin_proc(uint8_t** args);
int32_t builtin_pids(uint8_t** args);
int32_t builtin_pinfo(uint8_t** args);

// ============================================================
// BUILTIN DISPATCH TABLE
// ============================================================
static const Builtin BUILTINS[] = {
    { "debug",   builtin_debug},
    { "prompt",  builtin_prompt},
    { "status",  builtin_status},
    { "exit",    builtin_exit },
    { "help",    builtin_help},
    { "print",   builtin_print},
    { "echo",    builtin_echo},
    { "len",     builtin_len},
    { "sum",     builtin_sum},
    { "calc",    builtin_calc},
    { "basename",builtin_basename},
    { "dirname", builtin_dirname},
    { "dirch",   builtin_dirch},
    { "dirwd",   builtin_dirwd},
    { "dirmk",   builtin_dirmk},
    { "dirrm",   builtin_dirrm},
    { "dirls",   builtin_dirls},
    {"rename",   builtin_rename},
    {"unlink",   builtin_unlink},
    {"remove",   builtin_remove},
    {"linkhard", builtin_linkhard},
    {"linksoft", builtin_linksoft},
    {"linkread", builtin_linkread},
    {"linklist", builtin_linklist},
    {"cpcat",    builtin_cpcat},
    {"pid",      builtin_pid},
    {"ppid",     builtin_ppid},
    {"uid",      builtin_uid},
    {"euid",     builtin_euid},
    {"gid",      builtin_gid},
    {"egid",     builtin_egid},
    {"sysinfo",  builtin_sysinfo},
    {"proc",     builtin_proc},
    {"pids",     builtin_pids},
    {"pinfo",    builtin_pinfo},
};
static const int32_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

// ============================================================
// RAW MODE
// ============================================================
static struct termios ORIGINAL_TERMIOS;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMIOS);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &ORIGINAL_TERMIOS);
    atexit(disable_raw_mode); // nujno, spomni se kaj je blo brez tega lol

    struct termios raw = ORIGINAL_TERMIOS;
    /*
     * ECHO - disable echo - kernel ne izpisuje vec kar tipkas ampak je handlano v real_line_interactive
     * ICANON - disable canonical input - kernel neha bufferat vrstice in vsak znak gre direktno v program
     * IEXTEN - disable extended input char processing
     */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL); // IXON-enable start/stop output control, ICRNL-CRtoNL
    raw.c_cc[VMIN]  = 1; // blokiraj dokler se ne pojavi tocno 1 byte
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static int32_t read_line_interactive(uint8_t* buf, int32_t maxlen) {
    int32_t accumulated_len = 0;
    int32_t hist_idx = (int32_t)HISTORY_POSITION;
    memset(buf, 0, maxlen);

    while (1) {
        uint8_t c;
        if (read(STDIN_FILENO, &c, 1) <= 0) return 0;  // EOF

        // rocno handlanje: polsji ukaz
        if (c == '\n' || c == '\r') {
            buf[accumulated_len++] = '\n';
            buf[accumulated_len]   = '\0';
            write(STDOUT_FILENO, "\n", 1);
            return accumulated_len;
        }

        // backspace
        if (c == 127 || c == 8) {
            if (accumulated_len > 0)
            {
                accumulated_len--;
                buf[accumulated_len] = '\0';
                // clear char logika je <move back, print space, move back>
                write(STDOUT_FILENO, "\b \b", 3);
            }
            continue;
        }

        // zacetek escape sekvence
        if (c == '\033') {
            uint8_t seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] != '[') continue;

            // A -> nazaj zgodovina, B -> naprej (proti novejsemu) zgodovina
            const int32_t new_idx = hist_idx + (seq[1] == 'A' ? -1 : seq[1] == 'B' ? 1 : 0);
            if (new_idx < 0 || new_idx > (int32_t)HISTORY_POSITION) continue;

            for (int32_t i = 0; i < accumulated_len; i++) write(STDOUT_FILENO, "\b \b", 3);

            hist_idx = new_idx;
            if (hist_idx == (int32_t)HISTORY_POSITION) {
                accumulated_len = 0; buf[0] = '\0';
            } else {
                const uint8_t* entry = HISTORY[hist_idx % HISTORY_MAXLEN];
                accumulated_len = (int32_t)strlen((const char*)entry);
                if (accumulated_len > 0 && entry[accumulated_len-1] == '\n')
                {
                    accumulated_len--;
                }
                if (accumulated_len >= maxlen - 1)
                {
                    accumulated_len = maxlen - 2;
                }
                memcpy(buf, entry, accumulated_len);
                buf[accumulated_len] = '\0';
            }
            write(STDOUT_FILENO, buf, accumulated_len);
            continue;
        }

        if (accumulated_len < maxlen - 2) {
            buf[accumulated_len++] = c;
            buf[accumulated_len] = '\0';
            write(STDOUT_FILENO, &c, 1);
        }
    }
}


// ============================================================
// FUNCTION DEFINITIONS
// ============================================================

// -- helpers --------------------------------------------------
static inline void print_shell(void) {
    if (IS_INTERACTIVE)
    {
        fflush(stderr);
        printf("%s>", (char*)PROMPT);
        fflush(stdout);
    }
}

void print_command_args(uint8_t** args, const int32_t start, const uint8_t* separator, const bool new_line) {
    if (args == NULL) return;
    for (int32_t i = start; args[i] != NULL; i++)
    {
        printf("%s", (char*)args[i]);
        if (args[i+1] != NULL && separator != NULL)
        {
            printf("%s", (const char*)separator);
        }
    }
    if (new_line)
    {
        printf("\n");
    }
}

int32_t argument_count(uint8_t** args) {
    int32_t i = 0;
    while (args != NULL && args[i] != NULL) {i++;}
    return i;
}

void free_command(const Command* cmd) {
    if (cmd->args == NULL) return;

    int32_t i = 0;
    while (cmd->args[i] != NULL)
    {
        free(cmd->args[i++]);
    }
    free(cmd->args);
    free(cmd->redirect_in);
    free(cmd->redirect_out);
}

static inline bool is_word_char(const uint8_t c) {
    return isgraph(c) || c > 127;
}

static uint8_t* expand_tilde(const uint8_t* str) {
    if (str[0] != '~')
    {
        return (uint8_t*)strdup((const char*)str);
    }

    const uint8_t* home = (uint8_t*)getenv("HOME");
    if (home == NULL)
    {
        return (uint8_t*)strdup((const char*)str);
    }

    if (str[1] == '\0' || str[1] == '/') {
        // "~" ali "~/xyz"
        uint8_t* result = (uint8_t*)malloc(strlen((const char*)home) + strlen((const char*)str) + 1 + 1);
        strcpy((char*)result, (const char*)home);
        strcat((char*)result, (const char*)str + 1);  // skip ~
        return (uint8_t*)result;
    }

    return (uint8_t*)strdup((const char*)str);  // ~xyz - ni handlano
}

static int32_t comp_asc_int32_t(const void* a, const void* b) {
    const int32_t x = *(const int32_t*)a;
    const int32_t y = *(const int32_t*)b;
    return (x > y) - (x < y);
}

// -- lexer ----------------------------------------------------
void emit_token(const int32_t symbol_start, const int32_t symbol_end, const TokenType tt, const uint8_t* buffer, TokenList* token_list) {
    assert(symbol_start <= symbol_end);

    if (token_list->count == token_list->capacity)
    {
        token_list->capacity = token_list->capacity ? token_list->capacity * 2 : 8;
        Token* temp = realloc(token_list->tokens, token_list->capacity * sizeof(Token));
        if (temp == NULL)
        {
            perror("Error allocating space for temp");
            exit(1);
        }
        token_list->tokens = temp;
    }

    Token new_token = {.type = tt, .str_val = NULL};
    if (buffer != NULL)
    {
        new_token.str_val = (uint8_t*)strndup((const char*)&buffer[symbol_start], symbol_end - symbol_start);
    } else
    {
        new_token.str_val = (uint8_t*)strndup("EOF", 4);
    }
    token_list->tokens[token_list->count++] = new_token;
}

void lexer(const uint8_t* buffer, TokenList* token_list) {
    LexerState state = STATE_START;
    int32_t symbol_start = 0;
    for (int32_t i = 0; buffer[i] != '\0'; i++)
    {
        const uint8_t c = buffer[i];

        switch (state)
        {
        case STATE_START: {
            if (c == '#')
            {
                state = STATE_IN_COMMENT;
            } else if (c == '"')
            {
                state = STATE_IN_DOUBLE_QUOTES;
                symbol_start = i + 1;
            } else if (is_word_char(c))
            {
                state = STATE_WORD;
                symbol_start = i;
            } else if (isspace(c))
            {
                state = STATE_START;
            }
            break;
        }
        case STATE_WORD: {
            if (is_word_char(c))
            {
                state = STATE_WORD;
            } else if (isspace(c) || c == '\n')
            {
                state = STATE_START;
                emit_token(symbol_start, i, TOKEN_SYMBOL, buffer, token_list);
            }
            break;
        }
        case STATE_IN_COMMENT: {
            if (c == '\n')
            {
                state = STATE_START;
            }
            break;
        }
        case STATE_IN_DOUBLE_QUOTES: {
            if (c == '"')
            {
                state = STATE_START;
                emit_token(symbol_start, i, TOKEN_SYMBOL, buffer, token_list);
            } else if (c == '\n')
            {
                fprintf(stderr, "Missing closing \"\n");
                exit(1);
            }
            break;
        }
        default: {
            break;
        }
        }
    }

    if (state == STATE_WORD)
    {
        const int32_t len = (int32_t)strlen((const char*)buffer);
        emit_token(symbol_start, len, TOKEN_SYMBOL, buffer, token_list);
    }
    else if (state == STATE_IN_DOUBLE_QUOTES)
    {
        fprintf(stderr, "Missing closing \"\n");
        exit(1);
    }

    emit_token(0, 0, TOKEN_EOF, NULL, token_list);
}

// -- parser ---------------------------------------------------
void parser(const TokenList* token_list) {
    bool run_in_bg = false, redirect_output = false, redirect_input = false;

    for (int32_t i = token_list->count - 1 - 1; i >= token_list->count - 3 - 1 && i >= 0; i--)
    {
        Token* curr = &token_list->tokens[i];
        if (curr->str_val && curr->str_val[0] == '&')
        {
            if (run_in_bg || redirect_input || redirect_output)
            {
                fprintf(stderr, "Excessive use of 'run in background'\n");
                exit(1);
            }
            curr->type = TOKEN_RUN_IN_BACKGROUND;
            run_in_bg = true;
        } else if (curr->str_val && curr->str_val[0] == '>')
        {
            if (redirect_output || redirect_input)
            {
                fprintf(stderr, "Excessive use of 'redirect output' OR incorrect optional parameters order\n");
                exit(1);
            }
            curr->type = TOKEN_REDIRECT_OUTPUT;
            redirect_output = true;
        } else if (curr->str_val && curr->str_val[0] == '<')
        {
            if (redirect_input)
            {
                fprintf(stderr, "Excessive use of 'redirect input' OR incorrect optional parameters order\n");
                exit(1);
            }
            curr->type = TOKEN_REDIRECT_INPUT;
            redirect_input = true;
        }
    }
}

// -- commands -------------------------------------------------
Command build_command(const TokenList* token_list) {
    Command cmd = {0};
    cmd.args = malloc((token_list->count) * sizeof(uint8_t*));
    int32_t count = 0;
    for (int32_t i = 0; i < token_list->count; i++)
    {
        switch (token_list->tokens[i].type)
        {
        case TOKEN_SYMBOL:
            cmd.args[count++] = expand_tilde(token_list->tokens[i].str_val); // poglej ce se ustreza pogojem za expand "~" drugace handlaj normalno
            break;
        case TOKEN_REDIRECT_INPUT:
            cmd.redirect_in = expand_tilde(token_list->tokens[i].str_val+1); // +1 => minus >/<
            break;
        case TOKEN_REDIRECT_OUTPUT:
            cmd.redirect_out = expand_tilde(token_list->tokens[i].str_val+1);
            break;
        case TOKEN_RUN_IN_BACKGROUND:
            cmd.run_in_bg = true;
            break;
        case TOKEN_EOF:
            break;
        }
    }
    cmd.args[count] = NULL;

    return cmd;
}

builtin_fn find_builtin(const uint8_t* command_str) {
    for (int32_t i = 0; i < BUILTIN_COUNT; i++)
    {
        if (strcmp(BUILTINS[i].name, (char*)command_str) == 0)
        {
            return BUILTINS[i].fn;
        }
    }
    return NULL;
}

int32_t builtin_debug(uint8_t** args) {
    const int32_t arg_cnt = argument_count(args);
    if (arg_cnt > 1)
    {
        uint8_t* end;
        int32_t lvl = (int32_t)strtol((const char*)args[1], (char**)&end, 10);
        if (end == args[1] || *end != '\0' || errno == ERANGE)
        {
            if (DEBUG_LVL == DEEP_DEBUG_MODE)
            {
                fprintf(stderr, "Invalid lvl number for built-in `debug`, expected (positive) number, got %s\n", (char*)args[1]);
            }
            lvl = 0;
        }

        DEBUG_LVL = lvl;
    } else
    {
        printf("%d\n", DEBUG_LVL);
    }

    return 0;
}

int32_t builtin_prompt(uint8_t** args) {
    const int32_t arg_cnt = argument_count(args);
    if (arg_cnt > 1)
    {
        if (strlen((const char*)args[1]) > 8)
        {
            return 1;
        }

        if (PROMPT_IS_HEAP) free(PROMPT);
        PROMPT = (uint8_t*)strdup((const char*)args[1]);
        PROMPT_IS_HEAP = true;
    } else
    {
        printf("%s\n", (char*)PROMPT);
    }

    return 0;
}

int32_t builtin_status(uint8_t** args) {
    printf("%d\n", LAST_EXIT_STATUS);
    (void)args;
    return LAST_EXIT_STATUS;
}

int32_t builtin_exit(uint8_t** args) {
    uint8_t* end;
    const int32_t arg_cnt = argument_count(args);
    if (arg_cnt > 1)
    {
        const int32_t status = (int32_t)strtol((const char*)args[1], (char**)&end, 10);
        if (end == args[1] || *end != '\0' || errno == ERANGE)
        {
            if (DEBUG_LVL == DEEP_DEBUG_MODE)
            {
                fprintf(stderr, "Invalid exit status for built-in `exit`, expected (positive) number, got %s\n", (char*)args[1]);
            }
            LAST_EXIT_STATUS = -1;
        } else
        {
            LAST_EXIT_STATUS = status;
        }
    }

    exit(LAST_EXIT_STATUS);
}

int32_t builtin_help(uint8_t** args) {
    printf("CURRENT COMMANDS: debug, prompt, status, exit, help\n");
    (void)args;
    return 0;
}

int32_t builtin_print(uint8_t** args) {
    print_command_args(args, 1, (uint8_t*)" ", false);
    return 0;
}

int32_t builtin_echo(uint8_t** args) {
    print_command_args(args, 1, (uint8_t*)" ", true);
    return 0;
}

int32_t builtin_len(uint8_t** args) {
    int32_t total_len = 0;
    for (int32_t i = 1; args != NULL && args[i] != NULL; i++)
    {
        total_len += (int32_t)strlen((const char*)args[i]);
    }
    printf("%d\n", total_len);
    return 0;
}

int32_t builtin_sum(uint8_t** args) {
    int64_t sum = 0;
    for (int32_t i = 1; args != NULL && args[i] != NULL; i++)
    {
        uint8_t* end;
        const int64_t num = strtoll((const char*)args[i], (char**)&end, 10);
        if (end == args[i] || *end != '\0' || errno == ERANGE)
        {
            if (DEBUG_LVL == DEEP_DEBUG_MODE)
            {
                fprintf(stderr, "Invalid number as argument for built-in `sum`, expected numbers, got %s. Used 0 instead.\n", (char*)args[i]);
            }
            return -1;
        }
        sum += num;
    }
    printf("%lld\n", (long long int)sum);
    return 0;
}

int32_t builtin_calc(uint8_t** args) {
    if (args == NULL || argument_count(args) < 4) {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: calc <num> <op> <num>\n");
        }
        return 1;
    }

    const int64_t a = strtol((const char*)args[1], NULL, 10);
    const int64_t b = strtol((const char*)args[3], NULL, 10);
    const uint8_t op = args[2][0];

    int64_t result = 0;
    switch (op)
    {
    case '+': result = a + b; break;
    case '-': result = a - b; break;
    case '*': result = a * b; break;
    case '/':
        if (b == 0) {
            if (DEBUG_LVL == DEEP_DEBUG_MODE)
            {
                fprintf(stderr, "Division by zero\n");
            }
            return 1;
        }
        result = a / b;
        break;
    case '%':
        if (b == 0) {
            if (DEBUG_LVL == DEEP_DEBUG_MODE)
            {
                fprintf(stderr, "Division by zero\n");
            }
            return 1;
        }
        result = a % b;
        break;
    default:
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Unknown operator\n");
        }
        return 1;
    }

    printf("%lld\n", (long long int)result);
    return 0;
}

int32_t builtin_basename(uint8_t** args) {
    if (args == NULL || argument_count(args) < 2 || args[1] == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: basename <arg>\n");
        }
        return 1;
    }

    if (strcmp("/", (const char*)args[1]) == 0)
    {
        printf("/\n");
        return 0;
    }

    bool started = false;
    int32_t end = 0, start = 0;

    int32_t len = (int32_t)strlen((const char*)args[1]);
    while (len > 1 && args[1][len - 1] == '/') // skipamo trailing (/)*
    {
        len--;
    }

    for (int32_t i = len - 1; i >= 0; i--)
    {
        if (args[1][i] != '/')
        {
            if (started)
            {
                // nadaljuj
            } else
            {
                started = true;
                end = i;
            }
        }
        if (args[1][i] == '/')
        {
            // konec
            start = i + 1;
            break;
        }
    }
    printf("%.*s\n", end-start+1, (const char*)&args[1][start]);
    return 0;
}

int32_t builtin_dirname(uint8_t** args) {
    if (args == NULL || argument_count(args) < 2 || args[1] == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: dirname <arg>\n");
        }
        return 1;
    }

    int32_t len = (int32_t)strlen((const char*)args[1]);
    while (len > 1 && args[1][len - 1] == '/') len--;  // stripamo trailing /-e

    // desno proti levi iscemo zadnji /
    int32_t slash_pos = -1;
    for (int32_t i = len - 1; i >= 0; i--)
    {
        if (args[1][i] == '/')
        {
            slash_pos = i;
            break;
        }
    }

    if (slash_pos == -1)
    {
        // npr. foo -> "." (ce sploh ni /)
        printf(".\n");
    } else if (slash_pos == 0)
    {
        // / samo na rootu
        printf("/\n");
    } else
    {
        printf("%.*s\n", slash_pos, (const char*)args[1]);
    }

    return 0;
}

int32_t builtin_dirch(uint8_t** args) {
    if (args == NULL) return 1;

    const uint8_t* path = (uint8_t*)"/";

    if (args[1] != NULL)
    {
        path = args[1];
    }

    const int32_t ok = chdir((const char*)path);
    if (ok != 0)
    {
        const int32_t saved_errno = errno;
        perror("dirch");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_dirwd(uint8_t** args) {
    if (args == NULL) return 1;

    int32_t ret_status = 0;
    bool base_mode = true;
    if (args[1] != NULL)
    {
        if (strcmp((const char*)args[1], "full") == 0)
        {
            base_mode = false; // base==false => mode=full
        }
    }

    uint8_t* cwd = malloc(CWD_MAXLEN * sizeof(uint8_t));
    if (getcwd((char*)cwd, CWD_MAXLEN) == NULL)
    {
        const int32_t saved_errno = errno;
        perror("dirwd");
        free(cwd);
        return saved_errno;
    }

    if (base_mode)
    {
        uint8_t* pass_args[3];
        pass_args[0] = args[0];
        pass_args[1] = cwd;
        pass_args[2] = NULL;
        ret_status = builtin_basename(pass_args);
    } else
    {
        printf("%s\n", (const char*)cwd);
    }

    free(cwd);

    return ret_status;
}

int32_t builtin_dirmk(uint8_t** args) {
    if (args == NULL || argument_count(args) < 2)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: dirmk <dir>\n");
        }
        return 1;
    }

    if (mkdir((const char*)args[1], 0755) != 0)
    {
        const int32_t saved_errno = errno;
        perror("dirmk");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_dirrm(uint8_t** args) {
    if (args == NULL || argument_count(args) < 2)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: dirrm <dir>\n");
        }
        return 1;
    }

    if (rmdir((const char*)args[1]) != 0)
    {
        const int32_t saved_errno = errno;
        perror("dirrm");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_dirls(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: dirls <?dir>\n");
        }
        return 1;
    }

    const uint8_t* path = (uint8_t*)".";
    if (args[1] != NULL)
    {
        path = args[1];
    }

    int32_t saved_errno = 0;

    DIR* dir = opendir((const char*)path);
    if (dir == NULL)
    {
        saved_errno = errno;
        perror("dirls");
        return saved_errno;
    }

    while (1)
    {
        errno = 0;
        struct dirent* file = readdir(dir);
        if (file == NULL)
        {
            if (errno != 0)
            {
                saved_errno = errno;
                perror("dirls");
                closedir(dir);
                return saved_errno;
            }
            break;
        }
        printf("%s  ", file->d_name);
    }
    printf("\n");

    const int32_t close_dir_status = closedir(dir);
    if (close_dir_status != 0)
    {
        saved_errno = errno;
        perror("dirls");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_rename(uint8_t** args) {
    if (argument_count(args) < 3)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: rename <old> <new>\n");
        }
        return 1;
    }

    if (rename((const char*)args[1], (const char*)args[2]) < 0)
    {
        const int32_t saved_errno = errno;
        perror("rename");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_unlink(uint8_t** args) {
    if (argument_count(args) < 2)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: unlink <name>\n");
        }
        return 1;
    }

    if (unlink((const char*)args[1]) < 0)
    {
        const int32_t saved_errno = errno;
        perror("unlink");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_remove(uint8_t** args) {
    if (argument_count(args) < 2)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: remove <name>\n");
        }
        return 1;
    }

    if (remove((const char*)args[1]) < 0)
    {
        const int32_t saved_errno = errno;
        perror("remove");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_linkhard(uint8_t** args) {
    if (argument_count(args) < 3)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: linkhard <dst> <name>\n");
        }
        return 1;
    }

    if (link((const char*)args[1], (const char*)args[2]) < 0)
    {
        const int32_t saved_errno = errno;
        perror("linkhard");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_linksoft(uint8_t** args) {
    if (argument_count(args) < 3)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: linksoft <dst> <name>\n");
        }
        return 1;
    }

    if (symlink((const char*)args[1], (const char*)args[2]) < 0)
    {
        const int32_t saved_errno = errno;
        perror("linksoft");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_linkread(uint8_t** args) {
    if (argument_count(args) < 2)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: linkread <name>\n");
        }
        return 1;
    }

    int32_t saved_errno = 0;

    uint8_t* buff = malloc(LINKREAD_MAXLEN * sizeof(uint8_t));
    if (buff == NULL)
    {
        saved_errno = errno;
        perror("malloc");
        return saved_errno;
    }

    int32_t read_bytes = 0;
    if ((read_bytes = (int32_t)readlink((const char*)args[1], (char*)buff, LINKREAD_MAXLEN)) < 0)
    {
        saved_errno = errno;
        perror("linkread");
        free(buff);
        return saved_errno;
    }

    printf("%.*s\n", read_bytes, (const char*)buff);

    free(buff);
    return 0;
}

int32_t builtin_linklist(uint8_t** args) {
    if (argument_count(args) < 2)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: linklist <name>\n");
        }
        return 1;
    }

    int32_t saved_errno = 0;

    struct stat target_stat;

    if (stat((const char*)args[1], &target_stat) < 0)
    {
        saved_errno = errno;
        perror("linklist");
        return saved_errno;
    }

    DIR* dir = opendir(".");
    if (dir == NULL)
    {
        saved_errno = errno;
        perror("linklist");
        return saved_errno;
    }

    while (1)
    {
        errno = 0;
        const struct dirent* file = readdir(dir);
        if (file == NULL)
        {
            if (errno != 0)
            {
                saved_errno = errno;
                perror("linklist");
                const int32_t close_dir_status = closedir(dir);
                if (close_dir_status != 0)
                {
                    saved_errno = errno;
                    perror("linklist");
                    return saved_errno;
                }
                return saved_errno;
            }
            break;
        }

        struct stat s;
        if (stat(file->d_name, &s) < 0)
        {
            saved_errno = errno;
            perror("linklist");
            const int32_t close_dir_status = closedir(dir);
            if (close_dir_status != 0)
            {
                saved_errno = errno;
                perror("linklist");
                return saved_errno;
            }
            return saved_errno;
        }
        //printf("%s:[%llu]  ", file->d_name, s.st_ino); // st_ino[o] = 31006706
        if (s.st_ino == target_stat.st_ino)
        {
            printf("%s  ", file->d_name);
        }
    }
    printf("\n");

    const int32_t close_dir_status = closedir(dir);
    if (close_dir_status != 0)
    {
        saved_errno = errno;
        perror("linklist");
        return saved_errno;
    }

    return 0;
}

int32_t builtin_cpcat(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: cpcat <src?> <dst?>\n");
        }
        return 1;
    }

    int32_t src = STDIN_FILENO;
    int32_t dst = STDOUT_FILENO;
    const int32_t arg_cnt = argument_count(args);
    int32_t saved_errno = 0;

    // source
    if (arg_cnt > 1)
    {
        if (strcmp((const char*)args[1], "-") != 0)
        {
            src = open((const char*)args[1], O_RDONLY);
            if (src < 0)
            {
                saved_errno = errno;
                perror("cpcat");
                return saved_errno;
            }
        }
    }
    // destination
    if (arg_cnt > 2)
    {
        dst = open((const char*)args[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (dst < 0)
        {
            saved_errno = errno;
            perror("cpcat");
            if (src != STDIN_FILENO)  close(src);
            return  saved_errno;
        }
    }

    uint8_t* buff = malloc(CPCAT_BUFF_MAXLEN * sizeof(uint8_t));
    if (buff == NULL)
    {
        saved_errno = errno;
        if (src != STDIN_FILENO)  close(src);
        if (dst != STDOUT_FILENO) close(dst);
        return saved_errno;
    }
    ssize_t read_bytes = 0;
    while ((read_bytes = read(src, buff, CPCAT_BUFF_MAXLEN)) > 0)
    {
        write(dst, buff, read_bytes);
    }

    if (src != STDIN_FILENO)  close(src);
    if (dst != STDOUT_FILENO) close(dst);

    free(buff);
    return 0;
}

int32_t builtin_pid(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: pid\n");
        }
        return 1;
    }

    printf("%d\n", getpid());

    return 0;
}

int32_t builtin_ppid(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: ppid\n");
        }
        return 1;
    }

    printf("%d\n", getppid());

    return 0;
}

int32_t builtin_uid(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: uid\n");
        }
        return 1;
    }

    printf("%d\n", getuid());

    return 0;
}

int32_t builtin_euid(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: euid\n");
        }
        return 1;
    }

    printf("%d\n", geteuid());

    return 0;
}

int32_t builtin_gid(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: gid\n");
        }
        return 1;
    }

    printf("%d\n", getgid());

    return 0;
}

int32_t builtin_egid(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: egid\n");
        }
        return 1;
    }

    printf("%d\n", getegid());

    return 0;
}

int32_t builtin_sysinfo(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: sysinfo\n");
        }
        return 1;
    }

    struct utsname u;
    if (uname(&u) < 0)
    {
        const int32_t saved_errno = errno;
        perror("sysinfo");
        return saved_errno;
    }

    printf(
        "Sysname: %s\n"
        "Nodename: %s\n"
        "Release: %s\n"
        "Version: %s\n"
        "Machine: %s\n",
        u.sysname,
        u.nodename,
        u.release,
        u.version,
        u.machine);

    return 0;
}

int32_t builtin_proc(uint8_t** args) {
    const int32_t arg_cnt = argument_count(args);
    if (arg_cnt == 0)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: proc <path?>\n");
        }
        return 1;
    }
    if (arg_cnt == 1)
    {
        printf("%s\n", (const char*)PROCFS_PATH);
        return 0;
    }

    if (access((const char*)args[1], F_OK | R_OK) < 0)
    {
        const int32_t saved_errno = errno;
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            perror("proc");
            return saved_errno;
        }
        // za preverjanje perror ni potreben
        return 1;
    }

    if (PROCFS_PATH_IS_HEAP) free(PROCFS_PATH);
    PROCFS_PATH = (uint8_t*)strdup((const char*)args[1]);
    PROCFS_PATH_IS_HEAP = true;

    return 0;
}

static int32_t* collect_sorted_pids(int32_t* count) {
    *count = 0;

    DIR* dir = opendir((const char*)PROCFS_PATH);
    if (dir == NULL)
    {
        perror("collect sorted pids");
        return NULL;
    }

    int32_t capacity = PIDS_ARR_INITIAL_CAPACITY;
    int32_t* pids = (int32_t*)malloc(capacity * sizeof(int32_t));
    if (pids == NULL)
    {
        closedir(dir);
        return NULL;
    }

    while (1) {
        errno = 0;
        const struct dirent* entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) { perror("procfs"); free(pids); closedir(dir); return NULL; }
            break;
        }

        bool is_pid = entry->d_name[0] != '\0';
        for (size_t i = 0; entry->d_name[i] != '\0'; i++) {
            if (!isdigit((unsigned char)entry->d_name[i]))
            {
                is_pid = false;
                break;
            }
        }

        if (is_pid) {
            if (*count >= capacity) {
                capacity *= 2;
                int32_t* temp = realloc(pids, capacity * sizeof(int32_t));
                if (temp == NULL)
                {
                    free(pids);
                    closedir(dir);
                    return NULL;
                }
                pids = temp;
            }
            pids[(*count)++] = (int32_t)strtol(entry->d_name, NULL, 10);
        }
    }

    closedir(dir);

    qsort(pids, *count, sizeof(pids[0]), comp_asc_int32_t);

    return pids;
}

int32_t builtin_pids(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: pids\n");
        }
        return 1;
    }

    int32_t pids_count = 0;
    int32_t* pids = collect_sorted_pids(&pids_count);
    if (pids == NULL)
    {
        perror("pids");
        return 1;
    }

    for (int32_t i = 0; i < pids_count; i++)
    {
        printf("%d\n", pids[i]);
    }

    free(pids);

    return 0;
}

int32_t builtin_pinfo(uint8_t** args) {
    if (args == NULL)
    {
        if (DEBUG_LVL == DEEP_DEBUG_MODE)
        {
            fprintf(stderr, "Usage: pinfo\n");
        }
        return 1;
    }

    int32_t pids_count = 0;
    int32_t* pids = collect_sorted_pids(&pids_count);
    if (pids == NULL)
    {
        perror("pinfo");
        return 1;
    }

    printf("%5s %5s %6s %s\n", "PID", "PPID", "STANJE", "IME");

    for (int32_t i = 0; i < pids_count; i++)
    {
        uint8_t path[64];
        snprintf((char*)path, sizeof(path), "%s/%d/stat", PROCFS_PATH, pids[i]);

        FILE* f = fopen((const char*)path, "r");
        if (f == NULL)
        {
            continue; // proces mogoce umru, ni nujno da je neki broken
        }

        int32_t pid, ppid;
        uint8_t state, name[256];
        fscanf(f, "%d (%255[^)]) %c %d", &pid, name, &state, &ppid);
        fclose(f);

        printf("%5d %5d %6c %s\n", pid, ppid, state, (const char*)name);
    }

    free(pids);

    return 0;
}

int32_t execute(const TokenList* token_list) {
    if (token_list->count == 0 || token_list->tokens[0].type == TOKEN_EOF) return 0;

    const Command cmd = build_command(token_list);
    const builtin_fn fn = find_builtin(cmd.args[0]);

    int32_t ret_status = 0;

    if (fn != NULL)// found built-in
    {
        if (DEBUG_LVL > 0)
        {
            printf("Executing builtin '%s' in %s\n", (char*)cmd.args[0], cmd.run_in_bg ? "background" : "foreground");
        }
        ret_status = fn(cmd.args);
    } else
    {
#ifdef NALOGA1_REPL_3
        printf("External command '");
        print_command_args(cmd.args, 0, (uint8_t*)" ", false);
        printf("'\n");
#endif
    }

    if (!cmd.run_in_bg)
    {
        LAST_EXIT_STATUS = ret_status;
    }

    free_command(&cmd);

    return ret_status;
}

int main(const int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0); // disable stdout buffering (fixa butast bug pri naloga2.2 ko se je perror izpisal na zacetku)
    IS_INTERACTIVE = isatty(STDIN_FILENO);

    const FILE* input;

    if (IS_INTERACTIVE || argc < 2) {
        input = stdin;
    } else
    {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            perror("Error opening file");
            exit(1);
        }
    }

    print_shell();

    uint8_t* buffer = malloc(INPUT_BUFFER_SIZE * sizeof(uint8_t));
    if (buffer == NULL)
    {
        perror("Error allocating space for buffer");
        exit(1);
    }

    if (IS_INTERACTIVE) enable_raw_mode();

    while (1)
    {
        bool got_line = false;
        if (IS_INTERACTIVE)
        {
            const int32_t n = read_line_interactive(buffer, INPUT_BUFFER_SIZE);
            if (n <= 0) break;
            got_line = true;
        } else
        {
            got_line = (fgets((char*)buffer, INPUT_BUFFER_SIZE, (FILE*)input) != NULL);
        }
        if (!got_line) break;


        TokenList token_list = {0};
        token_list.input_line = buffer;

        // zgodovina
        const uint32_t slot = HISTORY_POSITION % HISTORY_MAXLEN;
        free(HISTORY[slot]);
        HISTORY[slot] = (uint8_t*)strdup((const char*)buffer);
        HISTORY_POSITION++;

        lexer(buffer, &token_list);

        parser(&token_list);

        if (DEBUG_LVL > 0)
        {
#ifdef NALOGA1_REPL_1
            int32_t len = (int32_t)strlen((const char*)buffer);
            if (buffer[len - 1] == '\n') len--;
            printf("Input line: '%.*s'\n", len, (char*)buffer);
            for (int32_t i = 0; i < token_list.count; i++)
            {
                const Token t = token_list.tokens[i];
                if (t.type == TOKEN_EOF)
                {
                    continue;
                }
                printf("Token %d: '%s'\n", i, (char*)t.str_val);
            }
#endif
#ifdef NALOGA1_REPL_2
            for (int32_t i = token_list.count - 3 - 1; i < token_list.count && i >= 0; i++)
            {
                const Token curr = token_list.tokens[i];
                switch (curr.type)
                {
                case TOKEN_REDIRECT_INPUT: {
                    printf("Input redirect: '%s'\n", (char*)&curr.str_val[1]);
                    break;
                }
                case TOKEN_REDIRECT_OUTPUT: {
                    printf("Output redirect: '%s'\n", (char*)&curr.str_val[1]);
                    break;
                }
                case TOKEN_RUN_IN_BACKGROUND: {
                    printf("Background: 1\n");
                    break;
                }
                default: {
                    break;
                }
                }
            }
#endif
        }

        execute(&token_list);

        // cleanup after this command
        for (int32_t i = 0; i < token_list.count; i++)
        {
            free(token_list.tokens[i].str_val);
        }
        free(token_list.tokens);

        print_shell();
    }

    /* free */
    if (!IS_INTERACTIVE && argc >= 2)
    {
        fclose((FILE*)input);
    }

    free(buffer);

    const uint32_t filled = MIN(HISTORY_POSITION, HISTORY_MAXLEN);
    for (uint32_t i = 0; i < filled; i++)
    {
        free(HISTORY[i]);
    }

    exit(LAST_EXIT_STATUS);
}