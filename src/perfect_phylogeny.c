/*
  Copyright (C) 2014 by Gianluca Della Vedova


  You can redistribute this file and/or modify it
  under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Box is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this file
  If not, see <http://www.gnu.org/licenses/>.
*/


/**
   @file perfect_phylogeny.c
   @brief Implementation of @c perfect_phylogeny.h

*/
#include "perfect_phylogeny.h"
#ifdef TEST_EVERYTHING
#include <check.h>
#include <stdlib.h>
#endif

/**
   Pretty print a state.
   Mainly used for debug
*/
void log_state(const state_s* stp) {
        fprintf(stderr, "=======================================\n");
        fprintf(stderr, "State=");
        if (check_state(stp) > 0) fprintf(stderr, "NOT ");
        fprintf(stderr, "ok\n");
        fprintf(stderr, "  num_species: %d\n", stp->num_species);
        fprintf(stderr, "  num_characters: %d\n", stp->num_characters);
        fprintf(stderr, "  num_species_orig: %d\n", stp->num_species_orig);
        fprintf(stderr, "  num_characters_orig: %d\n", stp->num_characters_orig);

        fprintf(stderr, "------|-------|----------|------\n");
        fprintf(stderr, "      |current|          |      \n");
        fprintf(stderr, "  c   |states |characters|colors\n");
        fprintf(stderr, "------|-------|----------|------\n");
        for (size_t i = 0; i < stp->num_characters_orig; i++)
                fprintf(stderr, "%6d|%7d|%10d|%6d\n", i, stp->current_states[i], stp->characters[i], stp->colors[i]);
        fprintf(stderr, "------|-------|----------|------\n");

        fprintf(stderr, "------|-------\n");
        fprintf(stderr, "  s   |species\n");
        fprintf(stderr, "------|-------\n");
        for (size_t i = 0; i < stp->num_species_orig; i++) {
                fprintf(stderr, "%6d|%7d\n", i, stp->species[i]);
        }
        fprintf(stderr, "------|-------\n");

        fprintf(stderr, "  operation: %d\n", stp->operation);
        fprintf(stderr, "  realize: %d\n", stp->realize);

        fprintf(stderr, "  tried_characters. Address %p Values: ", stp->tried_characters);
        for(GSList *list=stp->tried_characters; list != NULL; list = g_slist_next(list))
                fprintf(stderr, "%d ", GPOINTER_TO_INT(list->data));
        fprintf(stderr, "\n");

        fprintf(stderr, "  character_queue. Address %p Values: ", stp->character_queue);
        for(GSList *list=stp->character_queue; list != NULL; list = g_slist_next(list))
                fprintf(stderr, "%d ", GPOINTER_TO_INT(list->data));
        fprintf(stderr, "\n");


        fprintf(stderr, "  Red-black graph. Address\n", &(stp->red_black));
        igraph_write_graph_edgelist(&(stp->red_black), stderr);
        fprintf(stderr, "\n");

        fprintf(stderr, "  Conflict graph. Address\n", &(stp->conflict));
        igraph_write_graph_edgelist(&(stp->conflict), stderr);
        fprintf(stderr, "\n");

}


/**
   \brief some functions to abstract the access to the instance matrix
*/

static uint32_t
matrix_get_value(state_s *stp, uint32_t species, uint32_t character) {
        return stp->matrix[character + stp->num_characters * species];
}

static void
matrix_set_value(state_s *stp, uint32_t species, uint32_t character, uint32_t value) {
        stp->matrix[character + stp->num_characters * species] = value;
}


static GSList* json_array2gslist(json_t* array) {
        GSList* list = NULL;
        size_t index;
        json_t *value;
        json_array_foreach(array, index, value)
                list = g_slist_prepend(list, GINT_TO_POINTER(json_integer_value(value)));
        return g_slist_reverse(list);
}

static uint32_t* json_array2array(json_t* array) {
        size_t index;
        json_t *value;
        uint32_t* new = GC_MALLOC(json_array_size(array) * sizeof(uint32_t));
        assert(new != NULL);
        json_array_foreach(array, index, value) {
                new[index] = json_integer_value(value);
        }
        return new;
}

static uint32_t json_get_integer(const json_t* root, const char* field) {
        json_t* obj = json_object_get(root, field);
        assert(obj != NULL && "Missing JSON field\n");
        assert(json_is_integer(obj) && "value must be an integer\n");
        return json_integer_value(obj);
}

static char* json_get_string(json_t* root, char* field) {
        json_t* obj = json_object_get(root, field);
        assert(obj != NULL && "Missing JSON field\n");
        assert(json_is_string(obj) && "value must be an integer\n");
        char* dst = GC_MALLOC(sizeof(char) * 300); // json_string_length
        dst = strdup(json_string_value(obj));
        return dst;
}

static uint32_t* json_get_array(json_t* root, char* field) {
        json_t* obj = json_object_get(root, field);
        assert(obj != NULL && "Missing JSON field\n");
        assert(json_is_array(obj) && "field must be an array\n");
        return json_array2array(obj);
}

static GSList* json_get_list(json_t* root, char* field, bool optional) {
        json_t* obj = json_object_get(root, field);
        if (!optional) {
                assert(obj != NULL && "Missing JSON field\n");
                assert(json_is_array(obj) && "field must be an array\n");
                return json_array2gslist(obj);
        } else
                return NULL;
}

void
read_state(const char* filename, state_s* stp) {
        json_set_alloc_funcs(GC_malloc, GC_free);
        json_error_t jerr;
        json_t* data = json_load_file(filename, JSON_DISABLE_EOF_CHECK, &jerr);
        assert(data != NULL && "Could not parse JSON file\n");

        init_state(stp, json_get_integer(data, "num_species_orig"), json_get_integer(data, "num_characters_orig"));

        stp->realize = json_get_integer(data, "realize");
        stp->tried_characters = json_get_list(data, "tried_characters", true);
        stp->character_queue = json_get_list(data, "character_queue", true);

        stp->num_species = json_get_integer(data, "num_species");
        stp->num_characters = json_get_integer(data, "num_characters");
        stp->current_states = json_get_array(data, "current");
        stp->species = json_get_array(data, "species");
        stp->characters = json_get_array(data, "characters");
        // Graphs
        FILE* fp;
        fp = fopen(json_get_string(data, "red_black_file"), "r");
        assert(fp != NULL && "Cannot open file\n");
        igraph_read_graph_graphml(&(stp->red_black), fp, 0);
        fclose(fp);

        fp = fopen(json_get_string(data, "conflict_file"), "r");
        assert(fp != NULL && "Cannot open file\n");
        igraph_read_graph_graphml(&(stp->conflict), fp, 0);
        fclose(fp);

        assert(check_state(stp) == 0);
}

static json_t* gslist2json_array(GSList* list) {
        json_t* array = json_array();
        for(;list != NULL; list = g_slist_next(list))
                json_array_append(array, json_integer(GPOINTER_TO_INT(list->data)));
        return array;
}

static json_t* array2json_array(uint32_t* p, size_t size) {
        json_t* array = json_array();
        for(size_t i=0; i<size; i++)
                json_array_append(array, json_integer(p[i]));
        return array;
}

/**
   Since we use JSON as exchange format, we first have to build the JSON object
   representing the state.

   Since the igraph library provides the  \c igraph_write_graph_graphml
   and \c igraph_read_graph_graphml functions that read/write a graphml file (an
   XML that is quite readable), instead of encoding the graphs in JSON, we
   include only the filenames and we export the graphs in GraphML
*/
static json_t* build_json_state(const state_s* stp, const char* redblack_filename, const char* conflict_filename) {
        json_set_alloc_funcs(GC_malloc, GC_free);
        assert(check_state(stp) == 0);
        json_t* data = json_object();
        assert(!json_object_set(data, "realize", json_integer(stp->realize)));
        assert(!json_object_set(data, "tried_characters", gslist2json_array(stp->tried_characters)));
        assert(!json_object_set(data, "character_queue", gslist2json_array(stp->character_queue)));
        assert(!json_object_set(data, "num_species", json_integer(stp->num_species)));
        assert(!json_object_set(data, "num_characters", json_integer(stp->num_characters)));
        assert(!json_object_set(data, "num_species_orig", json_integer(stp->num_species_orig)));
        assert(!json_object_set(data, "num_characters_orig", json_integer(stp->num_characters_orig)));
        if (stp->matrix != NULL)
                assert(!json_object_set(data, "matrix", array2json_array(stp->matrix, stp->num_species * stp->num_characters)));
        assert(!json_object_set(data, "current", array2json_array(stp->current_states, stp->num_characters_orig)));
        assert(!json_object_set(data, "species", array2json_array(stp->species, stp->num_species_orig)));
        assert(!json_object_set(data, "characters", array2json_array(stp->characters, stp->num_characters_orig)));
        assert(!json_object_set(data, "red_black_file", json_string(redblack_filename)));
        assert(!json_object_set(data, "conflict_file", json_string(conflict_filename)));

        return data;
}

void
write_state(const char* filename, state_s* stp) {
        json_set_alloc_funcs(GC_malloc, GC_free);
        char* rb_filename = strdup("");
        char* c_filename = strdup("");
        FILE* fp = NULL;
        asprintf(&rb_filename, "%s-redblack.graphml", filename);
        fp = fopen(rb_filename, "w");
        assert(fp != NULL);
        assert(!igraph_write_graph_graphml(&(stp->red_black), fp, true));
        fclose(fp);
        asprintf(&c_filename, "%s-conflict.graphml", filename);
        fp = fopen(c_filename, "w");
        assert(!igraph_write_graph_graphml(&(stp->conflict), fp, true));
        fclose(fp);
        assert(check_state(stp) == 0);
        json_t* data = build_json_state(stp, rb_filename, c_filename);
        free(rb_filename);
        free(c_filename);

        assert(!json_dump_file(data, filename, JSON_INDENT(4) | JSON_SORT_KEYS) && "Cannot write JSON file\n");
}


static uint32_t state_cmp(const state_s *stp1, const state_s *stp2) {
        uint32_t result = 0;
        if (stp1->num_characters != stp2->num_characters) result += 1;
        if (stp1->num_species != stp2->num_species) result += 2;
        if (stp1->num_characters_orig != stp2->num_characters_orig) result += 4;
        if (stp1->num_species_orig != stp2->num_species_orig) result += 8;
        if (stp1->current_states == NULL || stp2->current_states == NULL)  result += 16;
        else for (size_t i = 0; i < stp2->num_characters_orig; i++)
                     if (stp1->current_states[i] != stp2->current_states[i]) {
                             result += 16;
                             break;
                     }
        if (stp1->species == NULL || stp2->species == NULL)  result += 32;
        else for (size_t i = 0; i < stp2->num_species_orig; i++)
                     if (stp1->species[i] != stp2->species[i]) {
                             result += 32;
                             break;
                     }
        if (stp1->characters == NULL || stp2->characters == NULL)  result += 64;
        else for (size_t i = 0; i < stp2->num_characters_orig; i++)
                     if (stp1->characters[i] != stp2->characters[i]) {
                             result += 64;
                             break;
                     }
        return result;
}

void
full_copy_state(state_s* dst, const state_s* src) {
        copy_state(dst, src);
        dst->character_queue = g_slist_copy(src->character_queue);
        dst->tried_characters = g_slist_copy(src->tried_characters);
}


void
copy_state(state_s* dst, const state_s* src) {
        assert(dst != NULL);
        assert(check_state(src) == 0);
        init_state(dst, src->num_species_orig, src->num_characters_orig);
        dst->realize = src->realize;
        dst->tried_characters = NULL;
        dst->character_queue = NULL;
        dst->num_species = src->num_species;
        dst->num_characters = src->num_characters;
        igraph_copy(&(dst->red_black), &(src->red_black));
        igraph_copy(&(dst->conflict), &(src->conflict));
        dst->matrix = src->matrix;
        for (size_t i = 0; i < src->num_characters_orig; i++) {
                dst->current_states[i] = src->current_states[i];
                dst->characters[i] = src->characters[i];
                dst->colors[i] = src->colors[i];
        }
        for (size_t i = 0; i < src->num_species_orig; i++) {
                dst->species[i] = src->species[i];
        }
        dst->operation = src->operation;
        assert(check_state(dst) == 0);
        assert(state_cmp(dst, src) == 0);
}

/**
   To realize a character, first we have to find the id \c c of the vertex of
   the red-black graph encoding the input character.
   Then we find the connected component \c A of the red-black graph to which \c
   c belongs, and the set \c B of vertices adjacent to \c c.

   If \c is labeled black, we remove all edges from \c c to \c B and we add the
   edges from \c c to \c A. Finally, we label \c c as red.

   If \c c is already red, we check that A=B. In that case we remove all edges
   incident on \c c and we remove the vertex \c c (since it is free). On the
   other hand, if A is not equal to B, we return that the realization is
   impossible, setting \c error=1.

*/
bool realize_character(state_s* dst, const state_s* src) {
        assert (src != NULL);
        assert (dst != NULL);
        copy_state(dst, src);
        /* printf("=====================\n"); */
        /* json_t* p_src = build_json_state(src, "", ""); */
        /* char* pp = json_dumps(p_src, JSON_SORT_KEYS | JSON_INDENT(2)); */
        /* printf("%s\n", pp); */
        assert(state_cmp(src, dst) == 0);
        assert(check_state(src) == 0);
        if (log_debug("realize_character"))
                log_state(src);
        uint32_t character = src->realize;
        /* json_t* p_dst = build_json_state(dst, "", ""); */
        /* pp = json_dumps(p_dst, JSON_SORT_KEYS | JSON_INDENT(2)); */
        /* printf("%s\n", pp); */
        /* assert(dst->current_states[0] == src->current_states[0]); */
        assert(check_state(dst) == 0);
        igraph_integer_t c = (igraph_integer_t) src->num_species_orig + character;
        int color = src->colors[character];
        int ret = 0;

        igraph_vector_t conn_comp;
        igraph_vector_init(&conn_comp, 1);
        ret = igraph_subcomponent(&(dst->red_black), &conn_comp, c, IGRAPH_ALL);
        assert(ret == 0);
        igraph_vector_sort(&conn_comp);

        igraph_vector_t adjacent;
        igraph_vector_init(&adjacent, 1);
        ret = igraph_neighbors(&(dst->red_black), &adjacent, c, IGRAPH_ALL);
        assert(ret == 0);
        igraph_vector_t not_adjacent;
        igraph_vector_init(&not_adjacent, 0);
        igraph_vector_t temp;
        igraph_vector_init(&temp, 1);
        igraph_vector_difference_sorted(&conn_comp, &adjacent, &temp);
        igraph_vector_t new_red;
        size_t l=igraph_vector_size(&temp);
        igraph_vector_init(&new_red, 0);
        for (size_t i=0; i<l; i++) {
                uint32_t v = VECTOR(temp)[i];
                /* check if v is a species */
                if (v < dst->num_species_orig && v != c) {
                        igraph_vector_push_back(&new_red, c);
                        igraph_vector_push_back(&new_red, v);
                        igraph_vector_push_back(&not_adjacent, v);
                }
        }
        igraph_es_t es;
        igraph_es_incident(&es, c, IGRAPH_ALL);
        igraph_delete_edges(&(dst->red_black), es);
        igraph_es_destroy(&es);
        log_debug("Trying to realize CHAR %d", character);
        assert(check_state(dst) == 0);
        if (color == BLACK) {
                log_debug("color %d = BLACK", color);
                igraph_add_edges(&(dst->red_black), &new_red, 0);
                dst->operation = 1;
                dst->colors[character] = RED;
                dst->current_states[character] = 1;
        }
        if (color == RED) {
                /* igraph_vector_print(&adjacent); */
                log_debug("color %d = RED", color);
                if (igraph_vector_size(&not_adjacent) > 0) {
                        dst->operation = 0;
                        igraph_vector_destroy(&new_red);
                        igraph_vector_destroy(&temp);
                        igraph_vector_destroy(&not_adjacent);
                        igraph_vector_destroy(&adjacent);
                        igraph_vector_destroy(&conn_comp);
                        return false;
                } else {
                        dst->operation = 2;
                        dst->colors[character] = RED + 1;
                        delete_character(dst, character);
                }
        }
        dst->realize = character;
        if (log_debug("realized")) {
                log_debug("color %d", color);
                log_debug("outcome %d", dst->operation);
                log_state(dst);
        }
/* igraph_write_graph_edgelist(dst.red_black, stdout); */
        assert(check_state(dst) == 0);
        cleanup(dst);
        assert(check_state(dst) == 0);
        igraph_vector_destroy(&new_red);
        igraph_vector_destroy(&temp);
        igraph_vector_destroy(&not_adjacent);
        igraph_vector_destroy(&adjacent);
        igraph_vector_destroy(&conn_comp);
        return true;
}

/* static int */
/* conflict_graph_init(igraph_t *g, igraph_t *red_black, uint32_t num_species, uint32_t num_characters) { */
/*     /\* for(uint32_t c1=n; c1<n+m; c1++) *\/ */
/*     /\*     for(uint32_t c2=c1+1; c2<n+m; c2++) { *\/ */
/*     /\*         igraph_vector_ptr_t n1, n2; *\/ */
/*     /\*         igraph_neighborhood(red_black, n1, c1, 1, 0); *\/ */
/*     /\*         igraph_neighborhood(red_black, n2, c2, 1, 0); *\/ */
/*     /\*     } *\/ */

/*     return 0; */
/* } */



/**
   \brief read the file containing an instance of the ppp problem and computes the
   corresponding state
   \param filename stp

   \c stp is a pointer to an existing state

   Reads an instance from file. If \c global_props contains a \c NULL \c file,
   then also the first row of the file, storing the number of species and
   characters must be read.
   If the file contains no instances to be read, then the function returns \c NULL.

   Updates an instance by computing the red-black and the conflict graphs
   associated to a given matrix.

   In a red-black graph, the first \c stp->num_species ids correspond to species,
   while the ids larger or equal to stp->num_species correspond to characters.
   Notice that the label id must be conserved when modifying the graph (i.e.
   realizing a character).

   color attribute is \c SPECIES if the vertex is a species, otherwise it is \c BLACK
   or \c RED (at the beginning, there can only be \c BLACK edges).

*/
bool
read_instance_from_filename(instances_schema_s* global_props, state_s* stp) {
        assert(global_props->filename != NULL);
        if (global_props->file == NULL) {
                global_props->file = fopen(global_props->filename, "r");
                assert(global_props->file != NULL);
                assert(!feof(global_props->file));

                assert(fscanf(global_props->file, "%"SCNu32" %"SCNu32, &(global_props->num_species),
                              &(global_props->num_characters)) != EOF);
        }

        init_state(stp, global_props->num_species, global_props->num_characters);
        stp->num_species = global_props->num_species;
        stp->num_characters = global_props->num_characters;
        stp->matrix = GC_MALLOC(stp->num_species * stp->num_characters * sizeof(uint32_t));
        for(uint32_t s=0; s < stp->num_species; s++)
                for(uint32_t c=0; c < stp->num_characters; c++) {
                        uint32_t x;
                        assert(fscanf(global_props->file, "%"SCNu32, &x) != EOF || s == 0 && c == 0);
                        if (feof(global_props->file)) {
                                fclose(global_props->file);
                                return false;
                        }
                        matrix_set_value(stp, s, c, x);
                }

/* red-black graph */
        for (uint32_t s=0; s < stp->num_species; s++)
                for (uint32_t c=0; c < stp->num_characters; c++)
                        if (matrix_get_value(stp, s, c) == 1)
                                igraph_add_edge(&(stp->red_black), s, c + stp->num_species);

        /* conflict graph */
        for(uint32_t c1 = 0; c1 < stp->num_characters; c1++)
                for(uint32_t c2 = c1 + 1; c2 < stp->num_characters; c2++) {
                        uint32_t states[2][2] = { {0, 0}, {0, 0} };
                        for(uint32_t s=0; s < stp->num_species; s++)
                                states[matrix_get_value(stp, s, c1)][matrix_get_value(stp, s, c2)] = 1;
                        if(states[0][0] + states[0][1] + states[1][0] + states[1][1] == 4)
                                igraph_add_edge(&(stp->conflict), c1, c2);
                }

        assert(check_state(stp) == 0);
        if (log_debug("STATE"))
                log_state(stp);
        return true;
}

/*
  \brief Simplify the instance whenever possible.

  We remove null characters and species.
*/
void cleanup(state_s *stp) {
        log_debug("Cleanup");
        // Looking for null species
        for (uint32_t s=0; s < stp->num_species_orig; s++)
                if (stp->species[s]) {
                        igraph_es_t es;
                        igraph_integer_t size;
                        igraph_es_incident(&es, s, IGRAPH_ALL);
                        igraph_es_size(&(stp->red_black), &es, &size);
                        if (size == 0)
                                delete_species(stp, s);
                        igraph_es_destroy(&es);
                }

        // Looking for null characters
        for (uint32_t c = 0; c < stp->num_characters_orig; c++)
                if (stp->characters[c]) {
                        igraph_es_t es;
                        igraph_integer_t size;
                        igraph_es_incident(&es, stp->num_species_orig + c, IGRAPH_ALL);
                        igraph_es_size(&(stp->red_black), &es, &size);
                        if (size == 0)
                                delete_character(stp, c);
                        igraph_es_destroy(&es);
                }
/* TODO (if necessary) */
/* we remove duplicated characters */
/* we remove duplicated species */
}

igraph_t *
get_red_black_graph(const state_s *inst) {
        // TODO
        return NULL;
}

igraph_t *
get_conflict_graph(const state_s *inst) {
        // TODO
        return NULL;
}



void
free_state(state_s *stp) {
        assert(stp != NULL);

        log_debug("malloc delete %p %p", &(stp->red_black), &(stp->conflict));
        log_debug("Old graph %p\n", &(stp->red_black));
        igraph_destroy(&(stp->red_black));
        igraph_destroy(&(stp->conflict));

        g_slist_free(stp->tried_characters);
        g_slist_free(stp->character_queue);
}


void init_state(state_s *stp, uint32_t nspecies, uint32_t nchars) {
        assert(stp != NULL);
        stp->num_characters_orig = nchars;
        stp->num_species_orig = nspecies;
        stp->num_characters = nchars;
        stp->num_species = nspecies;
        stp->tried_characters = NULL;
        stp->character_queue = NULL;
        stp->realize = 0;
        log_debug("malloc new %p %p", &(stp->red_black), &(stp->conflict));
        stp->current_states = GC_MALLOC(nchars * sizeof(uint32_t));
        stp->species = GC_MALLOC(nspecies * sizeof(uint32_t));
        stp->characters = GC_MALLOC(nchars * sizeof(uint32_t));
        stp->colors = GC_MALLOC(nchars * sizeof(uint8_t));
        log_debug("New graph %p\n", &(stp->red_black));
        igraph_empty(&(stp->red_black), nspecies + nchars, IGRAPH_UNDIRECTED);
        igraph_empty(&(stp->conflict), nchars, IGRAPH_UNDIRECTED);

        stp->operation = 0;
        for (uint32_t i=0; i < stp->num_species_orig; i++) {
                stp->species[i] = 1;
        }
        for (uint32_t i=0; i < stp->num_characters_orig; i++) {
                stp->current_states[i] = 0;
                stp->characters[i] = 1;
                stp->colors[i] = BLACK;
        }
}

uint32_t check_state(const state_s* stp) {
        uint32_t err = 0;
        if (stp->num_species == -1 || stp->num_species > stp->num_species_orig) {
                err += 1;
                log_debug("__FUNCTION__@__FILE__: __LINE__ (%d != %d)", stp->num_species, 0);
        }
        if (stp->num_characters == -1 || stp->num_characters > stp->num_characters_orig) {
                err += 2;
                log_debug("__FUNCTION__@__FILE__: __LINE__ (%d != %d)", stp->num_characters, 0);
        }

        uint32_t count = 0;
        for (uint32_t s = 0; s < stp->num_species_orig; s++) {
                if (stp->species[s])
                        count++;
        }
        if (count != stp->num_species) {
                err += 4;
                log_debug("__FUNCTION__@__FILE__: __LINE__ (%d != %d)", stp->num_species, count);
        }

        count = 0;
        for (uint32_t c = 0; c < stp->num_characters_orig; c++) {
                if (stp->characters[c])
                        count++;
        }
        if (count != stp->num_characters) {
                err += 8;
                log_debug("__FUNCTION__@__FILE__: __LINE__ (%d != %d)", stp->num_characters, count);
        }

        count = 0;
        for (uint32_t c = 0; c < stp->num_characters_orig; c++) {
                if (stp->current_states[c] != -1)
                        count++;
        }
        if (count != stp->num_characters) {
                err += 16;
                log_debug("__FUNCTION__@__FILE__: __LINE__ (%d != %d)", stp->num_characters, count);
        }
        return err;
}

uint32_t
characters_list(state_s * stp, uint32_t *array) {
        assert(array != NULL);
        uint32_t size = 0;
        for (unsigned int color = 1; color <= MAX_COLOR; color++)
                for (uint32_t c=0; c < stp->num_characters_orig; c++)
                        if (stp->characters[c] == color)
                                list = g_slist_prepend(list, GINT_TO_POINTER(c));
        GSList* res = g_slist_reverse(list);
        return res;
}


void delete_species(state_s *stp, uint32_t s) {
        log_debug("Deleting species %d", s);
        assert(s < stp->num_species_orig);
        assert(stp->species[s] > 0);
        stp->species[s] = 0;
        (stp->num_species)--;
}

void delete_character(state_s *stp, uint32_t c) {
        log_debug("Deleting character %d", c);
        assert(c < stp->num_characters_orig);
        assert(stp->characters[c] > 0);
        assert(stp->current_states[c] != -1);
        stp->characters[c] = 0;
        stp->current_states[c] = -1;
        (stp->num_characters)--;
}


#ifdef TEST_EVERYTHING
/* START_TEST(write_json_2) { */
/*     state_s *stp = new_state(); */
/*     stp->realize = 1; */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(91)); */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(92)); */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(93)); */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(95)); */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(96)); */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(97)); */
/*     stp->tried_characters = g_slist_append(stp->tried_characters, GINT_TO_POINTER(98)); */
/*     write_state("tests/api/2.json", stp); */

/*     state_s *stp2 = read_state("tests/api/2.json"); */
/*     ck_assert_int_eq(stp->realize, stp2->realize); */
/*     for (size_t i=0; i<7; i++) */
/*         ck_assert_int_eq(GPOINTER_TO_INT(g_slist_nth_data(stp->tried_characters, i)), */
/*             GPOINTER_TO_INT(g_slist_nth_data(stp2->tried_characters, i))); */
/* } */
/* END_TEST */
START_TEST(realize_3_0) {
        state_s st;
        read_state("tests/api/3.json", &st);
        state_s st2;
        stp->realize = 0;
        realize_character(&st2, stp);
        write_state("tests/api/3-0.json", &st2);
        ck_assert_int_eq(st2.realize, 0);
}
END_TEST

START_TEST(write_json_3) {
        state_s st;
        instances_schema_s props = {
                .file = NULL,
                .filename = "tests/input/read/3.txt"
        };
        read_instance_from_filename(&props, &st);
        assert(check_state(&st) == 0);
        write_state("tests/api/3t.json", &st);

        state_s st2;
        read_state("tests/api/3t.json", &st2);
        ck_assert_int_eq(stp.realize, stp2.realize);
}
END_TEST

/* static int g_slist_cmp(GSList* l1, GSList* l2) { */
/*         if (l1 == NULL && l2 == NULL) return 0; */
/*         if (l1 == NULL) return 1; */
/*         if (l2 == NULL) return -1; */
/*         GSList* x1 = g_slist_next(l1); */
/*         GSList* x2 = g_slist_next(l2); */
/*         for (;x1 != NULL && x2 != NULL; x1 = g_slist_next(x1), x2 = g_slist_next(x2)) { */
/*                 int32_t d = GPOINTER_TO_INT(x2->data) - GPOINTER_TO_INT(x1->data); */
/*                 if (d != 0) return d; */
/*         } */
/*         if (x1 == NULL && x2 == NULL) return 0; */
/*         return (x1 == NULL) ? 1 : -1; */
/* } */
START_TEST(copy_state_1) {
        instances_schema_s props = {
                .file = NULL,
                .filename = "tests/input/read/1.txt"
        };
        state_s* st, st2;
        read_instance_from_filename(&props, &st);
        copy_state(&st2, %st);
        ck_assert_int_eq(state_cmp(&st, &st2),0);
}
END_TEST
START_TEST(copy_state_2) {
        instances_schema_s props = {
                .file = NULL,
                .filename = "tests/input/read/2.txt"
        };
        state_s* st, st2;
        read_instance_from_filename(&props, &st);
        copy_state(&st2, %st);
        ck_assert_int_eq(state_cmp(&st, &st2),0);
}
END_TEST

static Suite * perfect_phylogeny_suite(void) {
        Suite *s;
        TCase *tc_core;

        s = suite_create("perfect_phylogeny.c");

/* Core test case */
        tc_core = tcase_create("Core");

/* tcase_add_test(tc_core, reset_state_1); */
        tcase_add_test(tc_core, test_read_instance_from_filename_1);
        tcase_add_test(tc_core, test_read_instance_from_filename_2);
        tcase_add_test(tc_core, new_state_1);
/* tcase_add_test(tc_core, destroy_instance_1); */
        tcase_add_test(tc_core, copy_state_1);
        tcase_add_test(tc_core, copy_state_2);

        tcase_add_test(tc_core, test_read_instance_from_filename_3);
/* tcase_add_test(tc_core, write_json_1); */
/* tcase_add_test(tc_core, write_json_2); */
        tcase_add_test(tc_core, write_json_3);
        tcase_add_test(tc_core, realize_3_0);

        suite_add_tcase(s, tc_core);

        return s;
}

/**
   This file is mainly used as a library.

   The standalone file is used for testing. If it is invoked without arguments,
   it runs a battery of unit tests, defined with the *check* package.
   It is invoked with a filename argument, the filename is a json file
   describing the regression test.

   If the json file contains an empty list of characters to be realized, then
   cleanup the state.
*/
int main(int argc, char **argv) {
        if(argc < 2) {
                Suite *s;
                SRunner *sr;
                s = perfect_phylogeny_suite();
                sr = srunner_create(s);
                srunner_run_all(sr, CK_NORMAL);
                int number_failed = srunner_ntests_failed(sr);
                srunner_free(sr);
                return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        json_error_t error;
        json_t* data = json_load_file(argv[1], 0, &error);
        assert(data != NULL && "Could not parse JSON file\n");
        unsigned int test_type = json_integer_value(json_object_get(data,"test"));
        if (test_type == 1) {
                const char *input_json_filename = json_string_value(json_object_get(data,"input"));
                json_t* listc = json_object_get(data, "characters");
                size_t index;
                json_t *value;
                state_s st;
                read_state(input_json_filename, &st);
                assert(check_state(stp) == 0);
                if (json_array_size(listc) > 0)
                        json_array_foreach(listc, index, value) {
                                state_s* st2;
                                stp->realize = json_integer_value(value);
                                realize_character(&st2, stp);
                                copy_state(stp, &st2);
                                assert(check_state(stp) == 0);
                        }
                else
                        cleanup(stp);
                assert(check_state(stp) == 0);
                write_state(json_string_value(json_object_get(data,"output")), stp);
        }
        return EXIT_SUCCESS;
}
#endif
