#include "shell.h"

// ---------- Storage: a singly linked list of name/value pairs ----------
typedef struct var {
    char*       name;
    char*       value;
    struct var* next;
} var_t;

static var_t* var_head = NULL;

static var_t* find_var(const char* name) {
    for (var_t* v = var_head; v != NULL; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

// Creates the variable, or replaces its value if it already exists.
// The list keeps its own copies (strdup) because the caller's strings
// are freed after every command.
void set_var(const char* name, const char* value) {
    var_t* v = find_var(name);

    if (v != NULL) {                       // already exists: replace the value
        char* copy = strdup(value);
        free(v->value);
        v->value = copy;
        return;
    }

    v = malloc(sizeof(var_t));
    v->name  = strdup(name);
    v->value = strdup(value);
    v->next  = NULL;

    // append at the end so 'set' lists variables in creation order
    if (var_head == NULL) {
        var_head = v;
    } else {
        var_t* tail = var_head;
        while (tail->next != NULL) tail = tail->next;
        tail->next = v;
    }
}

// Looks in the shell variables first, then in the environment
// (so $HOME and $USER work). Returns NULL if not defined anywhere.
const char* get_var(const char* name) {
    var_t* v = find_var(name);
    if (v != NULL) return v->value;
    return getenv(name);
}

// The 'set' built-in
void print_vars(void) {
    if (var_head == NULL) {
        printf("(no variables defined)\n");
        return;
    }
    for (var_t* v = var_head; v != NULL; v = v->next)
        printf("%s=%s\n", v->name, v->value);
}

// ---------- Recognising an assignment ----------
static int is_name_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
static int is_name_char(char c) {
    return is_name_start(c) || (c >= '0' && c <= '9');
}

// True if word looks like NAME=value (valid name, '=' right after it)
int is_assignment(const char* word) {
    if (!is_name_start(word[0])) return 0;
    const char* p = word;
    while (is_name_char(*p)) p++;
    return *p == '=';
}

// Splits "NAME=value" at the first '=' and stores it
void assign_word(const char* word) {
    const char* eq = strchr(word, '=');
    char* name = strndup(word, (size_t)(eq - word));
    set_var(name, eq + 1);
    free(name);
}

// ---------- Expansion: $NAME and $? ----------

// Appends n bytes of s to the growing buffer *buf
static void append(char** buf, size_t* len, size_t* cap, const char* s, size_t n) {
    size_t need = *len + n + 1;
    if (need > *cap) {
        while (need > *cap) *cap *= 2;
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
}

// Returns a NEW heap string where every $NAME / $? in word is replaced.
static char* expand_word(const char* word) {
    size_t cap = 64, len = 0;
    char* out = malloc(cap);
    out[0] = '\0';

    const char* p = word;
    while (*p != '\0') {
        if (*p == '$' && p[1] == '?') {                  // $? = last status
            char num[16];
            snprintf(num, sizeof(num), "%d", last_exit_status);
            append(&out, &len, &cap, num, strlen(num));
            p += 2;
        }
        else if (*p == '$' && is_name_start(p[1])) {     // $NAME
            const char* start = p + 1;
            const char* end = start;
            while (is_name_char(*end)) end++;

            char* name = strndup(start, (size_t)(end - start));
            const char* value = get_var(name);
            free(name);

            if (value != NULL)                           // undefined -> empty
                append(&out, &len, &cap, value, strlen(value));
            p = end;
        }
        else {                                           // ordinary character
            append(&out, &len, &cap, p, 1);
            p++;
        }
    }
    return out;
}

// Replaces *sp by its expanded version (frees the old string)
static void expand_string(char** sp) {
    if (*sp == NULL || strchr(*sp, '$') == NULL)
        return;                                          // nothing to expand
    char* expanded = expand_word(*sp);
    free(*sp);
    *sp = expanded;
}

// Expands every argument and redirection filename of every command
void expand_pipeline(pipeline_t* p) {
    for (int i = 0; i < p->n_cmds; i++) {
        command_t* c = &p->cmds[i];
        for (int j = 0; j < c->argc; j++)
            expand_string(&c->argv[j]);
        expand_string(&c->infile);
        expand_string(&c->outfile);
    }
}