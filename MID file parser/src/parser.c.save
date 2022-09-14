/* Name, parser.c, CS 24000, Spring 2020
 * Last updated March 27, 2020
 */

/* Add any includes here */

#include "parser.h"

#include <string.h>
#include <assert.h>
#include <malloc.h>

#define HEADER_LENGTH (0x06)
#define HEADER_CHUNK "MThd"
#define TRACK_CHUNK "MTrk"
#define MAX_META_INDEX (128)
#define MAX_MIDI_INDEX (0xFF)

uint8_t g_latest_status;

/*
 * This function reads one byte from the given file.
 */

uint8_t read_byte(FILE *file_ptr) {
  struct read {
    uint8_t byte;
  };
  struct read address;
  fread(&address, sizeof(uint8_t), 1, file_ptr);
  return address.byte;
} /* read_byte() */

/*
 * This function parse the given mid file.
 */

song_data_t *parse_file(const char *path) {
  assert(path != NULL);
  FILE *file_ptr = NULL;
  file_ptr = fopen(path, "rb");
  assert(file_ptr != NULL);
  song_data_t *song = malloc(sizeof(song_data_t));
  song->path = malloc(strlen(path) + 1);
  strcpy(song->path, path);
  parse_header(file_ptr, song);
  song->track_list = NULL;
  for (int i = 0; i < song->num_tracks; i++) {
    parse_track(file_ptr, song);
  }
  char last_char = '\0';
  int end_file = fread(&last_char, 1, 1, file_ptr);
  assert(end_file == 0);
  fclose(file_ptr);
  file_ptr = NULL;
  return song;
} /* parse_file() */

/*
 * This function parse the header of the given mid file
 * into the given song.
 */

void parse_header(FILE *file_ptr, song_data_t *song) {

  /* chunk type */

  char type[5];
  fread(type, 4, 1, file_ptr);
  type[4] = '\0';
  assert(strcmp(type, HEADER_CHUNK) == 0);

  /* read length */

  uint8_t length_bytes[4];
  for (int i = 0; i < 4; i++) {
    length_bytes[i] = read_byte(file_ptr);
  }
  uint32_t length = end_swap_32(length_bytes);
  assert(length == HEADER_LENGTH);

  /* read format */

  uint8_t byte_1 = read_byte(file_ptr);
  uint8_t byte_2 = read_byte(file_ptr);
  uint8_t bytes_array[2];
  bytes_array[0] = byte_1;
  bytes_array[1] = byte_2;
  uint16_t format = end_swap_16(bytes_array);
  assert((format == 0) || (format == 1) || (format == 2));
  song->format = format;

  /* read ntrks */

  uint8_t ntrks_bytes[2];
  for (int i = 0; i < 2; i++) {
    ntrks_bytes[i] = read_byte(file_ptr);
  }
  uint16_t ntrks = end_swap_16(ntrks_bytes);
  song->num_tracks = ntrks;

  /* read division */

  uint8_t first_byte = read_byte(file_ptr);
  uint8_t second_byte = read_byte(file_ptr);
  if ((first_byte & 0x80) == 0) {
    song->division.uses_tpq = true;
    uint8_t bytes_arr[2];
    bytes_arr[0] = first_byte;
    bytes_arr[1] = second_byte;
    song->division.ticks_per_qtr = end_swap_16(bytes_arr);
  }
  else {
    song->division.uses_tpq = false;
    song->division.ticks_per_frame = first_byte;
    song->division.frames_per_sec = second_byte;
  }
} /* parse_header() */

/*
 * This function parse a track of the given mid file
 * into the given song.
 */

void parse_track(FILE *file_ptr, song_data_t *song) {

  /* chunk type */

  char type[5];
  fread(type, 4, 1, file_ptr);
  type[4] = '\0';
  assert(strcmp(type, TRACK_CHUNK) == 0);

  /* read length */

  uint8_t length_bytes[4];
  for (int i = 0; i < 4; i++) {
    length_bytes[i] = read_byte(file_ptr);
  }
  uint32_t length = end_swap_32(length_bytes);

  /* create track */

  track_t *track = malloc(sizeof(track_t));
  track->length = length;

  /* read event */

  event_node_t *head_event = NULL;
  event_node_t *current_event = NULL;
  uint32_t read_bytes = 0;
  do {
    uint32_t pos_before = ftell(file_ptr);
    event_node_t *event_node = malloc(sizeof(event_node_t));
    event_node->next_event = NULL;
    event_node->event = parse_event(file_ptr);
    if (head_event == NULL) {
      head_event = event_node;
    }
    else {
      current_event->next_event = event_node;
    }
    current_event = event_node;
    uint32_t pos_after = ftell(file_ptr);
    read_bytes = read_bytes + (pos_after - pos_before);
  } while (read_bytes < length);
    assert(read_bytes == length);

  /* assign event_list */

  track->event_list = head_event;

  /* create track node */

  track_node_t *track_node = malloc(sizeof(track_node_t));
  track_node->next_track = NULL;
  track_node->track = track;

  /* insert track node to track list */

  track_node_t *current_track = NULL;
  current_track = song->track_list;
  if (current_track == NULL) {
    song->track_list = track_node;
  }
  else {
    while (1 > 0) {
      assert(current_track != NULL);
      if (current_track->next_track == NULL) {
        current_track->next_track = track_node;
        break;
      }
      current_track = current_track->next_track;
    }
  }
} /* parse_track() */

/*
 * This function parse an event of the given mid file.
 */

event_t *parse_event(FILE *file_ptr) {
  event_t *event = malloc(sizeof(event_t));;
  event->delta_time = parse_var_len(file_ptr);
  uint8_t type = read_byte(file_ptr);
  if (type == SYS_EVENT_1) {
    event->type = SYS_EVENT_1;
    event->sys_event = parse_sys_event(file_ptr, type);
  }
  else if (type == SYS_EVENT_2) {
    event->type = SYS_EVENT_2;
    event->sys_event = parse_sys_event(file_ptr, type);
  }
  else if (type == META_EVENT) {
    event->type = META_EVENT;
    event->meta_event = parse_meta_event(file_ptr);
  }
  else {
    event->midi_event = parse_midi_event(file_ptr, type);
    event->type = event->midi_event.status;
  }
  return event;
} /* parse_event() */

/*
 * This function parse a system event of the given mid file.
 */

sys_event_t parse_sys_event(FILE *file_ptr, uint8_t type) {
  sys_event_t event;
  event.data_len = parse_var_len(file_ptr);
  event.data = NULL;
  if (event.data_len != 0) {
    event.data = calloc(sizeof(uint8_t), event.data_len);
    uint8_t *ptr = event.data;
    uint8_t temp = 0;
    for (int i = 0; i < event.data_len; i++) {
      temp = read_byte(file_ptr);
      *(ptr + i) = temp;
    }
  }
  return event;
} /* parse_sys_event() */

/*
 * This function parse a meta event of the given mid file.
 */

meta_event_t parse_meta_event(FILE *file_ptr) {
  meta_event_t event;
  uint8_t type = read_byte(file_ptr);
  assert(type < MAX_META_INDEX);
  assert(META_TABLE[type].name != NULL);
  event.name = META_TABLE[type].name;
  if (META_TABLE[type].data_len == 0) {
    event.data_len = parse_var_len(file_ptr);
  }
  else {
    event.data_len = read_byte(file_ptr);
    assert(event.data_len == META_TABLE[type].data_len);
  }

  event.data = NULL;
  if (event.data_len != 0) {
    event.data = calloc(sizeof(uint8_t), event.data_len);
    uint8_t *data_ptr = event.data;
    uint8_t temp = 0;
    for (int i = 0; i < event.data_len; i++) {
      temp = read_byte(file_ptr);
      *(data_ptr + i) = temp;
    }
  }
  return event;
} /* parse_meta_event() */

/*
 * This function parse a midi event of the given mid file.
 */

midi_event_t parse_midi_event(FILE *file_ptr, uint8_t status) {
  midi_event_t event;
  uint8_t first_data_byte = 0;
  int implicit = 0;

  /* check status */

  if ((status & 0x80) == 0) {
    first_data_byte = status;
    status = g_latest_status;
    implicit++;
  }
  else {
    g_latest_status = status;
  }

  /* access name */

  assert(status < MAX_MIDI_INDEX);
  event.status = status;
  event.name = MIDI_TABLE[status].name;
  event.data_len = MIDI_TABLE[status].data_len;

  /* store data */

  event.data = NULL;
  if (event.data_len != 0) {
    event.data = calloc(sizeof(uint8_t), event.data_len);
    uint8_t *data_ptr = event.data;
    uint8_t temp = 0;
    if (implicit == 0){
      temp = read_byte(file_ptr);
      first_data_byte = temp;
    }
    *data_ptr = first_data_byte;
    for (int i = 1; i < event.data_len; i++) {
      temp = read_byte(file_ptr);
      *(data_ptr + i) = temp;
    }
  }
  return event;
} /* parse_midi_event() */

/*
 * This function parse a variable length quantity of the given mid file.
 */

uint32_t parse_var_len(FILE *file_ptr) {
  uint8_t temp = 0x80;
  uint32_t result = 0;
  int end_loop = 1;
  while (end_loop != 0) {
    temp = read_byte(file_ptr);
    if ((temp & 0x80) == 0) {
    end_loop = 0;
    }
    temp = temp & 0x7F;
    result = result << 7;
    result = result + temp;
  }
  return result;
} /* parse_var_len() */

/*
 * This function finds the type of the given event.
 */

uint8_t event_type(event_t *event) {
  if ((event->type == SYS_EVENT_1) || (event->type == SYS_EVENT_2)) {
    return SYS_EVENT_T;
  }
  else if (event->type == META_EVENT) {
    return META_EVENT_T;
  }
  else {
    return MIDI_EVENT_T;
  }
} /* event_type() */

/*
 * This function frees the given song.
 */

void free_song(song_data_t *song) {
  free(song->path);
  song->path = NULL;
  free_track_node(song->track_list);
  free(song);
  song = NULL;
} /* free_song() */

/*
 * This function frees the given track node.
 */

void free_track_node(track_node_t *track_node) {
  track_node_t *current_node = track_node;
  track_node_t *next_node = track_node->next_track;
  while (1 > 0) {
    free_event_node(current_node->track->event_list);
    free(current_node->track);
    current_node->track = NULL;
    current_node->next_track = NULL;
    free(current_node);
    current_node = next_node;
    if (next_node == NULL) {
      break;
    }
    next_node = next_node->next_track;
  }
} /* free_track_node() */

/*
 * This function frees the given event node.
 */

void free_event_node(event_node_t *event_node) {
  event_node_t *current = event_node;
  event_node_t *next = event_node->next_event;
  while (1 > 0) {
    current->next_event = NULL;
    uint8_t type = event_type(current->event);
    if (type == SYS_EVENT_T) {
      current->event->sys_event.data_len = 0;
      free(current->event->sys_event.data);
      current->event->sys_event.data = NULL;
    }
    else if (type == META_EVENT_T) {
      current->event->meta_event.name = NULL;
      current->event->meta_event.data_len = 0;
      free(current->event->meta_event.data);
      current->event->meta_event.data = NULL;
    }
    else {
      current->event->midi_event.name = NULL;
      current->event->midi_event.status = 0;
      current->event->midi_event.data_len = 0;
      free(current->event->midi_event.data);
      current->event->midi_event.data = NULL;
    }
    current->event->delta_time = 0;
    current->event->type = 0;
    free(current->event);
    current->event = NULL;
    free(current);
    current = next;
    if (next == NULL) {
      break;
    }
    next = next->next_event;
  }
} /* free_event_node() */

/*
 * This function returns 2 bytes integer from the given bytes..
 */

uint16_t end_swap_16(uint8_t bytes[2]) {
  uint16_t result = 0;
  result = result + bytes[0];
  result = result << 8;
  result = result + bytes[1];
  return result;
} /* end_swap_16() */

/*
 * This function returns 4 bytes integer from the given bytes..
 */

uint32_t end_swap_32(uint8_t bytes[4]) {
  uint32_t result = 0;
  result = result + bytes[0];
  result = result << 8;
  result = result + bytes[1];
  result = result << 8;
  result = result + bytes[2];
  result = result << 8;
  result = result + bytes[3];
  return result;
} /* end_swap_32() */
