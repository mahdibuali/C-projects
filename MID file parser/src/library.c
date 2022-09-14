/* Name, library.c, CS 24000, Spring 2020
 * Last updated March 27, 2020
 */

/* Add any includes here */

#include "library.h"

#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <dirent.h>

tree_node_t *g_song_library = NULL;

/*
 * This function finds the tree node with the given song name.
 */

tree_node_t **find_parent_pointer(tree_node_t **root, const char *song_name) {
  assert(root != NULL);

  if (*root == NULL) {
    return NULL;
  }

  if (strcmp((*root)->song_name, song_name) == 0) {
    return root;
  }

  if (strcmp(song_name, (*root)->song_name) < 0) {
    return find_parent_pointer(&((*root)->left_child), song_name);
  }

  return find_parent_pointer(&((*root)->right_child), song_name);
} /* find_parent_pointer() */

/*
 * This function inserts the given tree node into the given tree.
 */

int tree_insert(tree_node_t **root, tree_node_t *new_node) {
  assert(root != NULL);
  assert(*root != NULL);
  if (strcmp((*root)->song_name, new_node->song_name) == 0) {
    return DUPLICATE_SONG;
  }
  else if (strcmp((*root)->song_name, new_node->song_name) > 0) {
    if ((*root)->left_child == NULL) {
      (*root)->left_child = new_node;
      return INSERT_SUCCESS;
    }
    else {
      tree_insert(&((*root)->left_child), new_node);
    }
  }
  else {
    if ((*root)->right_child == NULL) {
      (*root)->right_child = new_node;
      return INSERT_SUCCESS;
    }
    else {
      tree_insert(&((*root)->right_child), new_node);
    }
  }
} /* tree_insert() */

/*
 * This function finds the tree node that has a child
 * with the given song name.
 */

tree_node_t **find_root_pointer(tree_node_t **root, const char *song_name) {
  assert(root != NULL);
  if (*root == NULL) {
    return NULL;
  }
  if ((*root)->left_child != NULL) {
    if (strcmp((*root)->left_child->song_name, song_name) == 0) {
      return root;
    }
  }

  if ((*root)->right_child != NULL) {
    if (strcmp((*root)->right_child->song_name, song_name) == 0) {
      return root;
    }
  }

  if (strcmp(song_name, (*root)->song_name) < 0) {
    return find_root_pointer(&((*root)->left_child), song_name);
  }

  return find_root_pointer(&((*root)->right_child), song_name);
} /* find_root_pointer() */


/*
 * This function removes the tree node with the given song name
 * from the given tree.
 */

int remove_song_from_tree(tree_node_t **root, const char *song_name) {
  assert(root != NULL);
  assert(song_name != NULL);

  /* if remove root */

  if (strcmp((*root)->song_name, song_name) == 0) {

    /* get its childern */

    tree_node_t *left_child = (*root)->left_child;
    tree_node_t *right_child = (*root)->right_child;

    /* free the node */

    free_node(*root);

    /* insert its children */

    if (left_child != NULL) {
      *root = left_child;
      if (right_child != NULL) {
        tree_insert(root, right_child);
      }
    }
    else {
      if (right_child != NULL) {
        *root = left_child;
      }
      else {
        *root = NULL;
      }
    }
    return DELETE_SUCCESS;
  }

  /* find the song node */

  tree_node_t **root_node_ptr = find_root_pointer(root, song_name);
  if (root_node_ptr == NULL) {
    return SONG_NOT_FOUND;
  }
  tree_node_t *node_ptr = NULL;
  if ((*root_node_ptr)->left_child != NULL) {
    if (strcmp((*root_node_ptr)->left_child->song_name, song_name) == 0) {
      node_ptr = (*root_node_ptr)->left_child;
      (*root_node_ptr)->left_child = NULL;
    }
  }
  if ((*root_node_ptr)->right_child != NULL) {
    if (strcmp((*root_node_ptr)->right_child->song_name, song_name) == 0) {
      node_ptr = (*root_node_ptr)->right_child;
      (*root_node_ptr)->right_child = NULL;
    }
  }

  /* get its childern */

  tree_node_t *left_child = node_ptr->left_child;
  tree_node_t *right_child = node_ptr->right_child;

  /* free the node */

  free_node(node_ptr);

  /* insert its children */

  if (left_child != NULL) {
    tree_insert(root, left_child);
  }
  if (right_child != NULL) {
    tree_insert(root, right_child);
  }
  return DELETE_SUCCESS;
} /* remove_song_from_tree() */

/*
 * This function frees the given tree node.
 */

void free_node(tree_node_t *node) {
  node->song_name = NULL;
  free_song(node->song);
  node->left_child = NULL;
  node->right_child = NULL;
  free(node);
  node = NULL;
} /* free_node() */

/*
 * This function prints the given tree node's song name.
 */

void print_node(tree_node_t *node, FILE *file_ptr) {
  fprintf(file_ptr, "%s\n", node->song_name);
} /* print_node() */

/*
 * This function traverse the given tree pre order
 * and apply the given function.
 */

void traverse_pre_order(tree_node_t *root, void *data,
                        traversal_func_t function) {
  if (root == NULL) {
    return;
  }
  function(root, data);
  traverse_pre_order(root->left_child, data, function);
  traverse_pre_order(root->right_child, data, function);
} /* traverse_pre_order() */

/*
 * This function traverse the given tree in order
 * and apply the given function.
 */

void traverse_in_order(tree_node_t *root, void *data,
                       traversal_func_t function) {
  if (root == NULL) {
    return;
  }
  traverse_in_order(root->left_child, data, function);
  function(root, data);
  traverse_in_order(root->right_child, data, function);
} /* traverse_in_order() */

/*
 * This function traverse the given tree post order
 * and apply the given function.
 */

void traverse_post_order(tree_node_t *root, void *data,
                         traversal_func_t function) {
  if (root == NULL) {
    return;
  }
  traverse_post_order(root->left_child, data, function);
  traverse_post_order(root->right_child, data, function);
  function(root, data);
} /* traverse_post_order() */



/*
 * This function frees all nodes in the given tree.
 */

void free_library(tree_node_t *root) {
  if (root == NULL) {
    return;
  }
  free_library(root->left_child);
  free_library(root->right_child);
  free_node(root);
} /* free_library() */

/*
 * This function prints all off the given tree nodes' song name.
 */

void write_song_list(FILE *file_ptr, tree_node_t *root) {
  traverse_in_order(root, file_ptr, (traversal_func_t) print_node);
} /* write_song_list() */

/*
 * This function makes a library from the mid files in the given
 * directory path..
 */

void make_library(const char *path) {
  struct dirent *entry = NULL;
  DIR *directory = opendir(path);
  assert(directory != NULL);
  int root = 0;
  while ((entry = readdir(directory)) != NULL) {

    /* check if song */

    int name_len = strlen(entry->d_name);
    char song_name[name_len + 1];
    strcpy(song_name, entry->d_name);
    if ((song_name[name_len - 1] == 'd') && (song_name[name_len - 2] == 'i') &&
        (song_name[name_len - 3] == 'm') && (song_name[name_len - 4] == '.')) {

      /* create node */

      tree_node_t *node = malloc(sizeof(tree_node_t));
      node->left_child = NULL;
      node->right_child = NULL;

      /* allocate song */

      if (path[strlen(path) - 1] != '/') {
        int path_len = name_len + strlen(path) + 1;
        char song_path[path_len + 1];
        strcpy(song_path, path);
        song_path[strlen(path)] = '/';
        for (int i = strlen(path) + 1; i < path_len + 1; i++) {
          song_path[i] = song_name[i - (strlen(path) + 1)];
        }
        node->song = parse_file(song_path);

        /* song name */

        char *song_path_ptr = node->song->path;
        node->song_name = song_path_ptr + strlen(path) + 1;
      }
      else {
        int path_len = name_len + strlen(path);
        char song_path[path_len + 1];
        strcpy(song_path, path);
        for (int i = strlen(path); i < path_len + 1; i++) {
          song_path[i] = song_name[i - (strlen(path))];
        }
        node->song = parse_file(song_path);

        /* song name */

        char *song_path_ptr = node->song->path;
        node->song_name = song_path_ptr + strlen(path);
      }

      /* insert to library */

      if (root == 0) {
        g_song_library = node;
        root = 1;
      }
      else {
        tree_insert(&g_song_library, node);
      }
    }
  }
  closedir(directory);
  directory = NULL;
} /* make_library() */
