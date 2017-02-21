#include "heap.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

//--- Macro-constantes pour l'impression ---------------------------------------

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define PRINTN(c, n) for (long unsigned i = 0; i < n; ++i) { fputc(c, stdout); }

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//--- Bloc -------------- ------------------------------------------------------

typedef struct bloc bloc;

typedef struct bloc {
  size_t size;
  bool available;
  union {
    bloc *next; // when available = true
    char data;  // when available = false
  } u;
} bloc;

// Renvoie la taille de l'espace occupé par les informations d'en-tête
//    du bloc telles que le compteur de taille et l'indicateur de service
#define BLOC_INFOS_SIZE (sizeof(size_t) + sizeof(bool))

// Renvoie la taille d'allocation minimale pour une donnée.
#define BLOC_DATAS_SIZE_MIN (sizeof(bloc) - BLOC_INFOS_SIZE)

// --- Déclaration des fonctions locales ---------------------------------------

// bloc_get_next : renvoie l'adresse du bloc chainé au bloc p dans le cas où p
//    repère un bloc disponible, un pointeur nul sinon
static bloc * bloc_get_next(const bloc * p);

// bloc_set_next : si p est disponible, affecte q au champ next du bloc p,
//    affiche un message d'alerte sinon
static void bloc_set_next(bloc * p, bloc * x);

// bloc_get_size : renvoie la taille du bloc p
static size_t bloc_get_size(const bloc *p);

// bloc_get_size : affecte au bloc pointé par p la taille size
static void bloc_set_size(bloc *p, size_t size);

// bloc_is_available : renvoie true si le bloc pointé par p est disponible
static bool bloc_is_available(const bloc *p);

// bloc_set_available : affecte au bloc pointé par p l'indicateur de service b
static void bloc_set_available(bloc *p, bool b);

// bloc_empty : génère une fausse tête
static bloc * bloc_empty(void);

// bloc_free : libère la fausse tête.
// [!] Ici, ne libère en aucun cas les blocs éventuellements chainée à p
static void bloc_free(bloc * p);

// bloc_insert_head : insert le bloc p en tête de la liste s
static void bloc_insert_head(bloc *s, bloc *p);

// bloc_remove_occ : détache le bloc x de la liste de blocs s
static void bloc_remove_occ(bloc *s, const bloc *x);

// bloc_next_memory_bloc : renvoie l'adresse du bloc mémoire suivant
//    directement le bloc p.
static bloc * bloc_next_memory_bloc(bloc * p);

// bloc_get_data_ptr : renvoie l'adresse des données du bloc passé en paramètre.
static void * bloc_get_data_ptr(bloc * p);

// bloc_from_data_ptr : renvoie l'adresse du bloc relatif
//    aux données dont l'adresse est passée en paramètre
static bloc * bloc_from_data_ptr(void * ptr);

// --- Implantation des fonctions locales --------------------------------------

bloc * bloc_get_next(const bloc * p) {
  return p->available ? p->u.next : NULL;
}

void bloc_set_next(bloc * p, bloc * x) {
  if (p->available) {
    p->u.next = x;
  } else {
    fprintf(stderr, "the bloc isn't available, next can't be assigned");
  }
}

size_t bloc_get_size(const bloc *p) {
  return p->size;
}

void bloc_set_size(bloc *p, size_t size) {
  p->size = size;
}

bool bloc_is_available(const bloc *p) {
  return p->available;
}

void bloc_set_available(bloc *p, bool b) {
  p->available = b;
}

// -- Fonctionnalités de listes

bloc * bloc_empty(void) {
  bloc * p = (bloc *) malloc(sizeof(bloc));
  p->available = true;
  p->size = 0;
  return p;
}

void bloc_free(bloc * p) {
  free(p);
}

void bloc_insert_head(bloc *s, bloc *p) {
  p->u.next = s->u.next;
  s->u.next = p;
}

void bloc_remove_occ(bloc *s, const bloc *x) {
  bloc * p = s;
  bloc * q = s;
  // IB :
  // QC : nombre d'appels à bloc_get_next(q)
  while ((q = bloc_get_next(q)) != NULL) {
    if (q == x) {
      p->u.next = q->u.next;
      return;
    }
    p = q;
  }
}

// -- Fonctionnalités autres

bloc * bloc_next_memory_bloc(bloc * p) {
  return (bloc *) ((char *) p + p->size);
}

void * bloc_get_data_ptr(bloc * p) {
  // printf("diff = %lu\n", (char *) &(p->u.data) - (char *) p); // MAGIC #1
  return &(p->u.data);
}

bloc * bloc_from_data_ptr(void * ptr) {
  return (bloc *)((char *) ptr - 16u); // MAGIC #1
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -- heap ---------------------------------------------------------------------

#define FUN_FAILURE 1
#define FUN_SUCCESS 0

#define M_SIZE_DEFAUT 80

#ifndef M_SIZE
#define M_SIZE M_SIZE_DEFAUT
#endif

//--- Déclaration des fonctions et des ariables locales ------------------------

// freeblocs : implante la liste des blocs disponibles
static bloc * freeblocs;

// memory_réserve : repère l'adresse de la mémoire réserve
static char * memory_reserve;

// get_offer : stock l'adresse de la fonction utilisée pour obtenir une offre
static bloc * (*get_offer)(size_t size);

// m_get_first_fit : renvoie l'adresse du premier bloc de mémoire libre
//   pouvant accueillir un bloc de taille size. Dans le meilleur des cas,
//   l'opération demande un temps constant. au pire des cas, elle requiert un
//   temps proportionnel à la longueur de la liste des blocs disponibles
static bloc * m_get_first_fit(size_t size);

// m_get_best_fit : renvoie l'adresse du bloc de mémoire libre
//   pouvant accueillir un bloc de taille size qui est le mieux adapté.
//   L'opération demande un temps proportionnel à la longueur de la liste
//   des blocs disponibles dans tous les cas.
static bloc * m_get_best_fit(size_t size);

// m_aggregate : parcours la réserve mémoire en vue de la défragmenter
static void m_aggregate();

//--- Implantation des fonctions locales ---------------------------------------

bloc * m_get_first_fit(size_t size) {
  bloc * p = freeblocs;
  // IB : p repère l'adresse d'un bloc de la liste des blocs disponibles
  //      ou p est nul est aucun bloc ne remplit la condition recherchée
  // QC : nombre d'appels à bloc_get_next(p)
  while ((p = bloc_get_next(p)) != NULL && bloc_get_size(p) < size);
  return p;
}

bloc * m_get_best_fit(size_t size) {
  size_t d = SIZE_MAX;
  bloc * p = freeblocs;
  bloc * q = NULL;
  // IB : p repère l'adresse d'un bloc de la liste des blocs disponibles
  //      ou p est nul si tous les bloc on été parcourus
  //   && q repère le bloc de la liste des blocs disponible compris
  //      entre le premier élément de la liste freeblocs et p
  //      tel que sa taille la plus proche supérieur à size
  // QC : nombre d'appels à bloc_get_next(p)
  while ((p = bloc_get_next(p)) != NULL) {
    size_t p_size = bloc_get_size(p);
    if (p_size >= size && (p_size - size < d)) {
      d = p_size - size;
      q = p;
    }
  }
  return q;
}

void m_aggregate() {
  // flush freeblocs
  bloc_set_next(freeblocs, NULL);

  char * e = memory_reserve + M_SIZE;
  bloc * p = (bloc *) memory_reserve;
  bloc * q;

  // IB : p est compris entre e et memory_reserve et repère un bloc mémoire
  //   && les blocs disponibles allant de memory_reserve à p on été fusionnés
  //      et ajoutés à la liste des blocs disponibles
  // QC : e - p
  while ((char *) p < e) {
    if (bloc_is_available(p) == false) {
      p = bloc_next_memory_bloc(p);
    } else {
      q = bloc_next_memory_bloc(p);
      // IB : q est compris entre p et e et repère un bloc mémoire
      //   && p et q sont deux blocs consécutifs
      // QC : e - q
      while ((char *) q < e && bloc_is_available(q)) {
        bloc_set_size(p, bloc_get_size(p) + bloc_get_size(q));
        q = bloc_next_memory_bloc(q);
      }
      bloc_insert_head(freeblocs, p);
      p = q;
    }
  }
}

//--- Fonctions de heap --------------------------------------------------------

int m_reserve(void) {
  printf("m_reserve\n");

  memory_reserve = malloc(M_SIZE);
  if (memory_reserve == NULL) {
    return FUN_FAILURE;
  }

  bloc *p = (bloc *) memory_reserve;
  p->size = M_SIZE;
  p->available = true;
  p->u.next = NULL;


  printf("%zu\n", sizeof(bool));
  printf("%zu\n", sizeof(bloc));
  printf("%zu\n", BLOC_INFOS_SIZE);
  printf("%zu\n", sizeof(p->u));

  freeblocs = bloc_empty();
  if (freeblocs == NULL) {
    free(memory_reserve);
    return FUN_FAILURE;
  }
  bloc_set_next(freeblocs, p);

  get_offer = m_get_best_fit;

  return FUN_SUCCESS;
}

void m_dispose(void) {
  printf("m_dispose\n");
  bloc_free(freeblocs);
  free(memory_reserve);
  memory_reserve = NULL;
}

void m_toggle_offer(void) {
  if (get_offer == m_get_best_fit) {
    get_offer = m_get_first_fit;
    printf("HEAP    : switch to mode first offer\n");
  } else {
    get_offer = m_get_best_fit;
    printf("HEAP    : switch to mode best offer\n");
  }
}

void * m_alloc(size_t s) {
  size_t size = s;
  // test overflow
  if (size > SIZE_MAX - BLOC_INFOS_SIZE) {
    printf("m_alloc : overflow : type\n");
    return NULL;
  }
  if (size > M_SIZE - BLOC_INFOS_SIZE) {
    printf("m_alloc : overflow : size %lu exceed memory space\n", size);
    return NULL;
  }
  // adjust to min size
  if (size < BLOC_DATAS_SIZE_MIN) {
    size = BLOC_DATAS_SIZE_MIN;
  }
  // increment by header datas space
  size += BLOC_INFOS_SIZE;

  printf("m_alloc : size increased to %lu (+%lu)\n", size, size - s);

  bloc * p = get_offer(size);
  if (p == NULL) {
    m_aggregate();
    p = get_offer(size);
  }
  if (p == NULL) {
    return NULL;
  }
  if (bloc_get_size(p) - size < sizeof(bloc)) {
    // prendre le bloc libre entier
    size_t v_lastsize = size;
    size = bloc_get_size(p);
    bloc_remove_occ(freeblocs, p);
    if (size > v_lastsize) {
      printf("m_alloc : size increased to %lu (+%lu)\n",
          size, size - v_lastsize);
    }
  }
  bloc_set_size(p, bloc_get_size(p) - size);
  bloc * x = bloc_next_memory_bloc(p);
  bloc_set_size(x, size);
  bloc_set_available(x, false);
  return bloc_get_data_ptr(x);
}

void m_free(void * ptr) {
  if (ptr == NULL) {
    return;
  }
  bloc *p = bloc_from_data_ptr(ptr);
  // printf("M_FREE  : %p\n", (void *) p);
  bloc_set_available(p, true);
  bloc_insert_head(freeblocs, p);
}


static void m_print_bloc(const bloc * p) {
  printf(KYEL "%08lu", bloc_get_size(p));
  printf(KNRM "%d", bloc_is_available(p));
  size_t data_size = bloc_get_size(p) - BLOC_INFOS_SIZE;
  if (bloc_is_available(p)) {
    printf(KGRN);
    int offset = printf("%p", (void *)(bloc_get_next(p)));
    if (offset >= 0 && data_size > (size_t) offset) {
      PRINTN('-', data_size - (size_t) offset);
    }
  } else {
    printf(KBLU);
    char c = *((char *) bloc_get_data_ptr((bloc *) p));
    printf("-%c-", c);
    PRINTN('#', data_size - 3);
  }
  printf(KNRM);
}

void m_print() {
  bloc *p = (bloc *) memory_reserve;
  char *e = (char *) p + M_SIZE;
  while ((char *) p < e) {
    m_print_bloc(p);
    p = bloc_next_memory_bloc(p);
  }
  printf("\n");
}

