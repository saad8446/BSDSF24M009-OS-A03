#include "shell.h"

#define MAX_BLOCK_LINES 32

// The three parts of an if statement, collected line by line
typedef struct {
    char* cond[MAX_BLOCK_LINES];       int n_cond;
    char* then_part[MAX_BLOCK_LINES];  int n_then;
    char* else_part[MAX_BLOCK_LINES];  int n_else;
    int   has_else;
} if_block_t;

// Which part of the statement we are currently reading
enum { ST_COND, ST_THEN, ST_ELSE };

static int is_space(char c) { return c == ' ' || c == '\t'; }

// Returns a malloc'd copy of s without leading/trailing spaces
static char* trim_dup(const char* s) {
    while (is_space(*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && is_space(s[len - 1])) len--;
    return strndup(s, len);
}

// True if the first word of line is exactly 'word' (e.g. "if")
int starts_with_word(const char* line, const char* word) {
    while (is_space(*line)) line++;
    size_t n = strlen(word);
    return strncmp(line, word, n) == 0 &&
           (line[n] == '\0' || is_space(line[n]));
}

// True for an empty line or a line whose first character is '#'
int is_blank_or_comment(const char* line) {
    while (is_space(*line)) line++;
    return *line == '\0' || *line == '#';
}

// Stores 'text' in arr. Takes ownership of text (frees it on failure).
static int add_line(char* arr[], int* n, char* text) {
    if (*n == MAX_BLOCK_LINES) {
        fprintf(stderr, "error: too many lines in a block (max %d)\n", MAX_BLOCK_LINES);
        free(text);
        return 0;
    }
    arr[(*n)++] = text;
    return 1;
}

static void free_lines(char* arr[], int n) {
    for (int i = 0; i < n; i++) free(arr[i]);
}

static void free_block(if_block_t* b) {
    free_lines(b->cond, b->n_cond);
    free_lines(b->then_part, b->n_then);
    free_lines(b->else_part, b->n_else);
}

// Prints a syntax error, frees the line, and returns 0 (failure)
static int fail(char* line, const char* msg) {
    fprintf(stderr, "syntax error: %s\n", msg);
    free(line);
    return 0;
}

// Runs each stored line in order; returns the status of the last one
static int run_lines(char* arr[], int n) {
    int status = 0;
    for (int i = 0; i < n; i++)
        status = run_line(arr[i]);
    return status;
}

// Reads lines with the "> " prompt until the matching 'fi'.
// Each line is placed into the cond / then / else list depending on the
// current state. Returns 1 on success, 0 on a syntax error.
static int read_body(if_block_t* b) {
    int state = ST_COND;

    while (1) {
        char* raw = readline(PROMPT2);
        if (raw == NULL) {                       // Ctrl+D in the middle
            fprintf(stderr, "\nsyntax error: unexpected end of input (missing 'fi')\n");
            return 0;
        }
        add_history_entry(raw);

        if (is_blank_or_comment(raw)) {
            free(raw);
            continue;
        }
        char* line = trim_dup(raw);
        free(raw);

        if (strcmp(line, "then") == 0) {
            if (state != ST_COND)  return fail(line, "unexpected 'then'");
            if (b->n_cond == 0)    return fail(line, "missing condition after 'if'");
            state = ST_THEN;
            free(line);
        }
        else if (strcmp(line, "else") == 0) {
            if (state != ST_THEN)  return fail(line, "unexpected 'else'");
            state = ST_ELSE;
            b->has_else = 1;
            free(line);
        }
        else if (strcmp(line, "fi") == 0) {
            free(line);
            if (state == ST_COND) {
                fprintf(stderr, "syntax error: missing 'then'\n");
                return 0;
            }
            return 1;                            // statement is complete
        }
        else if (starts_with_word(line, "if")) {
            return fail(line, "nested 'if' statements are not supported");
        }
        else {
            int ok;
            if (state == ST_COND)      ok = add_line(b->cond, &b->n_cond, line);
            else if (state == ST_THEN) ok = add_line(b->then_part, &b->n_then, line);
            else                       ok = add_line(b->else_part, &b->n_else, line);
            if (!ok) return 0;
        }
    }
}

// first_line is the line the user typed, starting with the word "if".
void run_if_statement(const char* first_line) {
    if_block_t blk;
    memset(&blk, 0, sizeof(blk));

    // Whatever follows the word "if" on the first line is the condition
    const char* rest = first_line;
    while (is_space(*rest)) rest++;
    rest += 2;                                   // skip the word "if"
    if (!is_blank_or_comment(rest)) {
        if (!add_line(blk.cond, &blk.n_cond, trim_dup(rest))) {
            free_block(&blk);
            return;
        }
    }

    if (!read_body(&blk)) {                      // syntax error: run nothing
        free_block(&blk);
        return;
    }

    // Run the condition; its exit status picks the branch
    int status = run_lines(blk.cond, blk.n_cond);

    if (status == 0)
        run_lines(blk.then_part, blk.n_then);    // success -> then block
    else if (blk.has_else)
        run_lines(blk.else_part, blk.n_else);    // failure -> else block

    free_block(&blk);
}