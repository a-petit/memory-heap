#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "heap.h"

#define IDENTIFIER_NBR 25
#define IDENTIFIER_TEST(x) (isupper(x))
#define IDENTIFIER_TO_INDEX(x) ((char) x - 'A')
#define IDENTIFIER_FROM_INDEX(x) ('A' + x)

void * itop[IDENTIFIER_NBR];

// -----------------------------------------------------------------------------

static void usage(const char *progname);

static void print_err_and_forget_line(const char *s, int c);

typedef enum {
  CMD_HELP,
#ifdef STATUS
  CMD_STATUS,
#endif
  CMD_QUIT,
  CMD_TOGGLE,
  CMD_ALLOC,
  CMD_FREE,
  CMD__NBR
} cmd;

static void pgm_help(void);
#ifdef STATUS
static void pgm_status(void);
#endif
static void pgm_quit(void);
static void pgm_toggle(void);
static void pgm_alloc(const char identifier);
static void pgm_free(const char identifier);

typedef struct {
  const char label;
  const char *comment;
  int nparam;
  union {
    void (*v)(void);
    void (*r)(const char identifier);
  } action;
} scmd;

scmd acmd[] = {
  [CMD_HELP] = {
    .label = 'h',
    .comment = "display this help.",
    .nparam = 0,
    .action.v = pgm_help,
  },
#ifdef STATUS
  [CMD_STATUS] = {
    .label = 's',
    .comment = "display the program's variables status.",
    .nparam = 0,
    .action.v = pgm_status,
  },
#endif
  [CMD_QUIT] = {
    .label = 'q',
    .comment = "quit the simulation.",
    .nparam = 0,
    .action.v = pgm_quit,
  },
  [CMD_TOGGLE] = {
    .label = 't',
    .comment = "switch between best offer mode and first offer mode.",
    .nparam = 0,
    .action.v = pgm_toggle,
  },
  [CMD_ALLOC] = {
    .label = 'm',
    .comment = "followed by a positive integer n, assign to %s the address of\n"
      "        a dynamic variable of size n.",
    .nparam = 1,
    .action.r = pgm_alloc,
  },
  [CMD_FREE] = {
    .label = 'f',
    .comment = "free the dynamic variable referred by %s.",
    .nparam = 1,
    .action.r = pgm_free,
  }
};


// -----------------------------------------------------------------------------

#define PROMPT ">"

#define ARG__OPT_LONG "--"
#define ARG_HELP      "help"

int main(int argc, char *argv[]) {

  //  IB : 1 <= k && k <= argc
  //    && aucune des chaines argv[1] à argv[k - 1] n'est préfixe de
  //        ARG__OPT_LONG . ARG_HELP ou n'admet ARG__OPT_LONG . ARG_HELP[0]
  //        comme préfixe
  //  QC : k
  for (int k = 1; k < argc; ++k) {
    const char *a = argv[k];
    if (strlen(a) > strlen(ARG__OPT_LONG)
        && strlen(a) <= strlen(ARG__OPT_LONG ARG_HELP)
        && strncmp(a, ARG__OPT_LONG ARG_HELP, strlen(a)) == 0) {
      usage(argv[0]);
      return EXIT_SUCCESS;
    }
  }

  for (size_t r = 0; r < IDENTIFIER_NBR; ++r) {
    itop[r] = NULL;
  }

  pgm_help();

  m_reserve();
  m_print();

  int id;
  int c = '\n';
  //  IB : c == dernier caractère lu sur la ligne de commande du jeu
  //    && toutes les commandes passées ont été traitées sauf la dernière dont
  //        l'action est associée à c
  //  QC : nombre d'appels à getchar()
  do {
    if (isspace(c)) {
      if (c == '\n') {
        printf(PROMPT " ");
      }
    } else {
      scmd *p = acmd;
      //  IB : acmd <= p && p <= acmd + CMD__NBR
      //    && et(q -> label != c ; acmd <= q && q < p)
      //  QC : p - acmd
      while (p < acmd + CMD__NBR && p -> label != c) {
        ++p;
      }
      if (p == acmd + CMD__NBR) {
        print_err_and_forget_line("Unknown command", c);
      } else {
        if (p -> nparam == 0) {
          p -> action.v();
        } else {
          do {
            id = getchar();
          } while (isspace(id));

          if (!IDENTIFIER_TEST(id)) {
            print_err_and_forget_line("Unvalid identifier", id);
          } else {
            p->action.r((char) id);
          }
        }
      }
    }
    c = getchar();
  } while (c != EOF);
  printf("\n");
  return EXIT_FAILURE;
}

void usage(const char *progname) {
  printf("Usage: %s\n", progname);
  printf("Simulation program for heap memory management.\n");
  printf(
      "\n"
      "  --help  display this help and exit\n"
      "\n");
}

void print_err_and_forget_line(const char *s, int c) {
  fprintf(stderr, "*** %s: '%c'\n", s, c);
  while (getchar() != '\n')
    ;
  ungetc('\n', stdin);
}

#define HELP_IDENTIFIER "x"

void pgm_help(void) {
  printf("Identifiers are made of a single uppercase character\n");
  printf("In the following list of commands, %s mean any identifier.\n",
    HELP_IDENTIFIER);
  printf("\nCommands:\n");
  //  IB : acmd <= p && p <= acmd + CMD__NBR
  //    && (toutes les commandes pointée par q tel que acmd <= q && q < p ont
  //        été affichées)
  //  QC : p - acmd
  for (scmd *p = acmd; p < acmd + CMD__NBR; ++p) {
    printf("%3c ", p -> label);
    if (p -> nparam >= 1) {
      printf("%s ", HELP_IDENTIFIER);
    }
    printf("%*s  ", 2 - 2 * p -> nparam, "");
    printf(p -> comment, HELP_IDENTIFIER);
    printf("\n");
  }
  printf("\n");
}

#ifdef STATUS
void pgm_status(void) {
  printf("\nIdentifiers:\n");
  for (int i = 0; i < IDENTIFIER_NBR; ++i) {
    printf("%3c", IDENTIFIER_FROM_INDEX(i));
    printf("     ");
    printf("%p", itop[i]);
    printf("\n");
  }
  printf("\n");
}
#endif

void pgm_quit(void) {
  m_dispose();
  printf("\nEnd of simulation.\n");
  exit(EXIT_SUCCESS);
}

void pgm_toggle(void) {
  m_toggle_offer();
}

void pgm_alloc(const char identifier) {
  // printf("pgm_alloc : %c\n", identifier);
  int i = IDENTIFIER_TO_INDEX(identifier);

  if (itop[i] != NULL) {
    print_err_and_forget_line("identifier is allocated yet", identifier);
    return;
  }

  size_t s;
  if (fscanf(stdin, "%lu", &s) != 1) {
    print_err_and_forget_line("Unvalid size", ' ');
    return;
  }

  itop[i] = m_alloc(s);
  if (itop[i] == NULL) {
    printf("M_ALLOC : FAILURE\n");
  } else {
    *((char *) itop[i]) = identifier;
  }

  m_print();
}

void pgm_free(const char identifier) {
  // printf("pgm_free : %c\n", identifier);
  int i = IDENTIFIER_TO_INDEX(identifier);
  m_free(itop[i]);
  itop[i] = NULL;
  m_print();
}
