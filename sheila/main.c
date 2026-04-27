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
};
static const int32_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

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

    if (strlen((const char*)str) > 1 && (str[1] == '\0' || str[1] == '/')) {
        // "~" ali "~/xyz"
        char* result = malloc(strlen((const char*)home) + strlen((const char*)str));
        strcpy(result, (const char*)home);
        strcat(result, (const char*)str + 1);  // skip ~
        return (uint8_t*)result;
    }

    return (uint8_t*)strdup((const char*)str);  // ~xyz - ni handlano
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
            cmd.redirect_in = (uint8_t*)strdup((const char*)token_list->tokens[i].str_val+1); // +1 => minus >/<
            break;
        case TOKEN_REDIRECT_OUTPUT:
            cmd.redirect_out = (uint8_t*)strdup((const char*)token_list->tokens[i].str_val+1);
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
    printf("%lld\n", sum);
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

    printf("%lld\n", result);
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

    while (fgets((char*)buffer, INPUT_BUFFER_SIZE, (FILE*)input))
    {
        TokenList token_list = {0};
        token_list.input_line = buffer;

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

    exit(LAST_EXIT_STATUS);
}