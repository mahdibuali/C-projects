/* Name, alterations.c, CS 24000, Spring 2020
 * Last updated April 9, 2020
 */

/* Add any includes here */

#include "alterations.h"

#include <assert.h>
#include <malloc.h>
#include <string.h>

#define MIN_CHANNEL_STATUS (0x80)
#define MAX_NOTE_STATUS (0xAF)
#define MAX_CHANNEL_STATUS (0xEF)
#define MIN_OCTAVE (0)
#define MAX_OCTAVE (127)
#define PROGRAM_CHANGE "Program Change"
#define MAX_CHANNEL (16)

/*
 * This function changes the given event by the given octave.
 */

int change_event_octave(event_t *event, int *octave) {
  assert(event);
  assert(octave);

  /* check type */

  if ((event->type == SYS_EVENT_1) || (event->type == SYS_EVENT_2) ||
      (event->type == META_EVENT)) {
    return 0;
  }

  /* check name */

  if ((event->midi_event.status < MIN_CHANNEL_STATUS) ||
            (event->midi_event.status > MAX_NOTE_STATUS)) {
    return 0;
  }

  /* get first data byte and check range */

  int new_octave = *(event->midi_event.data) + (*octave *  OCTAVE_STEP);
  if ((new_octave < MIN_OCTAVE) || (new_octave > MAX_OCTAVE)) {
    return 0;
  }
  *(event->midi_event.data) = (uint8_t) new_octave;
  return 1;
} /* change_event_octave() */

/*
 * This function gets the variable length quantity value of the given number.
 */

unsigned long get_vlq(uint32_t number) {
  uint8_t bytes[5];
  bytes[4] = number & 0x7F;
  bytes[3] = (number >> 7) & 0x7F;
  bytes[3] = bytes[3] | 0x80;
  bytes[2] = (number >> 14) & 0x7F;
  bytes[2] = bytes[2] | 0x80;
  bytes[1] = (number >> 21) & 0x7F;
  bytes[1] = bytes[1] | 0x80;
  bytes[0] = (number >> 28) & 0x7F;
  bytes[0] = bytes[0] | 0x80;

  unsigned long result = 0;
  int first_index = 0;

  for (int i = 0; i < 5; i++) {
    if (bytes[i] == 0x80) {
      first_index++;
    }
    else {
      break;
    }
  }
  for (int i = first_index; i < 5; i++) {
    result = result + bytes[i];
    if (i != 4) {
      result = result << 8;
    }
  }
  return result;
} /* get_vlq() */

/*
 * This function gets the number of bytes for the given number.
 */

int num_of_bytes(unsigned long num) {
  int result = 0;
  do {
    result++;
    num = num >> 8;
  } while (num != 0);
  return result;
} /* num_of_bytes() */

/*
 * This function gets the number of bytes for the given number.
 */

int change_event_time(event_t *event, float *multi) {
  assert(event);
  assert(multi);
  int old_time = num_of_bytes(get_vlq(event->delta_time));
  event->delta_time = event->delta_time * *multi;
  int new_time = num_of_bytes(get_vlq(event->delta_time));
  return new_time - old_time;
} /* change_event_time() */

/*
 * This function maps the event's instrument to the table.
 */

int change_event_instrument(event_t *event, remapping_t table) {
  assert(event);

  /* check type */

  if ((event->type == SYS_EVENT_1) || (event->type == SYS_EVENT_2) ||
      (event->type == META_EVENT)) {
    return 0;
  }

  /* check name */

  if (strcmp(PROGRAM_CHANGE, event->midi_event.name) != 0) {
    return 0;
  }

  /* get first data byte and check range */

  *(event->midi_event.data) = table[*(event->midi_event.data)];
  return 1;
} /* change_event_instrument() */

/*
 * This function maps the event's note to the table.
 */

int change_event_note(event_t *event, remapping_t table) {
  assert(event);

  /* check type */

  if ((event->type == SYS_EVENT_1) || (event->type == SYS_EVENT_2) ||
      (event->type == META_EVENT)) {
    return 0;
  }

  /* check name */

  if ((event->midi_event.status < MIN_CHANNEL_STATUS) ||
            (event->midi_event.status > MAX_NOTE_STATUS)) {
    return 0;
  }

  *(event->midi_event.data) = table[*(event->midi_event.data)];
  return 1;
} /* change_event_note() */

/*
 * This function applies the given function to all events in the song.
 */

int apply_to_events(song_data_t *song, event_func_t func, void *data) {
  assert(song);
  assert(data);
  int result = 0;
  track_node_t *current_track = song->track_list;
  while (current_track != NULL) {
    event_node_t *current_event = current_track->track->event_list;
    while (current_event != NULL) {
      if (func(current_event->event, data) == 1) {
        result++;
      }
      current_event = current_event->next_event;
    }
    current_track = current_track->next_track;
  }
  return result;
} /* apply_to_events() */

/*
 * This function changes the octave for all events in the song.
 */

int change_octave(song_data_t *song, int octave) {
  assert(song);
  return apply_to_events(song, (event_func_t)change_event_octave,
                         (void *)(&octave));
} /* change_octave() */

/*
 * This function warps time for all events in the song.
 */

int warp_time(song_data_t *song, float multi) {
  assert(song);
  int result = 0;
  track_node_t *current_track = song->track_list;
  while (current_track != NULL) {
    event_node_t *current_event = current_track->track->event_list;
    int bytes_diff = 0;
    while (current_event != NULL) {
      bytes_diff = bytes_diff + change_event_time(current_event->event,
                                                  &multi);
      current_event = current_event->next_event;
    }
    current_track->track->length += bytes_diff;
    result += bytes_diff;
    current_track = current_track->next_track;
  }
  return result;
} /* warp_time() */

/*
 * This function remaps instrument for all events in the song.
 */

int remap_instruments(song_data_t *song, remapping_t table) {
  assert(song);
  return apply_to_events(song, (event_func_t)change_event_instrument,
                         (void *)(table));
} /* remap_instruments() */

/*
 * This function remaps note for all events in the song.
 */

int remap_notes(song_data_t *song, remapping_t table) {
  assert(song);
  return apply_to_events(song, (event_func_t)change_event_note,
                         (void *)(table));
} /* remap_notes() */

/*
 * This function duplicates the given track..
 */

track_node_t *duplicate_track(track_node_t *original_track) {
  assert(original_track);

  /* create track node */

  track_node_t *new_track_node = malloc(sizeof(track_node_t));
  assert(new_track_node);
  new_track_node->next_track = NULL;

  /* create track */

  new_track_node->track = malloc(sizeof(track_t));
  assert(new_track_node->track);
  new_track_node->track->length = original_track->track->length;

  /* create event list */

  event_node_t *head_event_node = NULL;
  event_node_t *prev_event_node = NULL;
  event_node_t *current_event_node = original_track->track->event_list;
  while (current_event_node) {

    /* create event node */

    event_node_t *new_event_node = malloc(sizeof(event_node_t));
    assert(new_event_node);
    new_event_node->next_event = NULL;
    if (head_event_node == NULL) {
      head_event_node = new_event_node;
    }
    if (prev_event_node) {
      prev_event_node->next_event = new_event_node;
    }

    /* create event */

    event_t *new_event = malloc(sizeof(event_t));
    assert(new_event);
    new_event->delta_time = current_event_node->event->delta_time;
    new_event->type = current_event_node->event->type;

    /* check type */

    if ((new_event->type == SYS_EVENT_1) ||
        (new_event->type == SYS_EVENT_1)) {
      new_event->sys_event.data_len =
      current_event_node->event->sys_event.data_len;
      new_event->sys_event.data = NULL;
      if (new_event->sys_event.data_len) {
        new_event->sys_event.data = calloc(sizeof(uint8_t),
                                           new_event->sys_event.data_len);
        assert(new_event->sys_event.data);
        for (int i = 0; i < new_event->sys_event.data_len; i++) {
          *(new_event->sys_event.data + i) =
          *(current_event_node->event->sys_event.data + i);
        }
      }
    }
    else if (new_event->type == META_EVENT) {
      new_event->meta_event.name =
      current_event_node->event->meta_event.name;
      new_event->meta_event.data_len =
      current_event_node->event->meta_event.data_len;
      new_event->meta_event.data = NULL;
      if (new_event->meta_event.data_len) {
        new_event->meta_event.data = calloc(sizeof(uint8_t),
        new_event->meta_event.data_len);
        assert(new_event->meta_event.data);
        for (int i = 0; i < new_event->meta_event.data_len; i++) {
          *(new_event->meta_event.data + i) =
          *(current_event_node->event->meta_event.data + i);
        }
      }
    }
    else {
      new_event->midi_event.name =
      current_event_node->event->midi_event.name;
      new_event->midi_event.status =
      current_event_node->event->midi_event.status;
      new_event->midi_event.data_len =
      current_event_node->event->midi_event.data_len;
      new_event->midi_event.data = NULL;
      if (new_event->midi_event.data_len) {
        new_event->midi_event.data = calloc(sizeof(uint8_t),
        new_event->midi_event.data_len);
        assert(new_event->midi_event.data);
        for (int i = 0; i < new_event->midi_event.data_len; i++) {
          *(new_event->midi_event.data + i) =
          *(current_event_node->event->midi_event.data + i);
        }
      }
    }

    /* assign event and update variable */

    new_event_node->event = new_event;
    current_event_node = current_event_node->next_event;
    prev_event_node = new_event_node;
  }
  new_track_node->track->event_list = head_event_node;
  return new_track_node;
} /* duplicate_track() */

/*
 * This function finds the smallest unused channel..
 */

int smallest_unused_channel(song_data_t *song) {
  assert(song);
  int channels[MAX_CHANNEL];
  for (int i = 0; i < MAX_CHANNEL; i++) {
    channels[i] = 0;
  }
  track_node_t *current_track = song->track_list;
  while (current_track != NULL) {
    event_node_t *current_event = current_track->track->event_list;
    while (current_event != NULL) {
      if (event_type(current_event->event) == MIDI_EVENT_T) {
        if ((current_event->event->midi_event.status >= 0x80) &&
            (current_event->event->midi_event.status <= 0xEF)) {
          int channel = current_event->event->midi_event.status & 0x0F;
          channels[channel] = 1;
          break;
        }
      }
      current_event = current_event->next_event;
    }
    current_track = current_track->next_track;
  }
  for (int i = 0; i < MAX_CHANNEL; i++) {
    if (channels[i] == 0) {
      return i;
    }
  }
  return -1;
} /* smallest_unused_channel() */

/*
 * This function adds round to the song.
 */

void add_round(song_data_t *song, int index, int octave,
               unsigned int delay, uint8_t instrument) {
  assert(song);
  assert(index < song->num_tracks);
  assert(song->format != 2);

  /* duplicate the track */

  track_node_t *current_track = song->track_list;
  for (int i = 0; i < index; i++) {
    current_track = current_track->next_track;
  }
  track_node_t *new_track = duplicate_track(current_track);

  /* insert track */

  current_track = song->track_list;
  while (1) {
    if (current_track->next_track == NULL) {
      current_track->next_track = new_track;
      break;
    }
    current_track = current_track->next_track;
  }

  /* change octave */

  event_node_t *current_event = new_track->track->event_list;
  while (current_event) {
    change_event_octave(current_event->event, &octave);
    current_event = current_event->next_event;
  }

  /* delay the first event D*/

  int old_time =
  num_of_bytes(get_vlq(new_track->track->event_list->event->delta_time));
  new_track->track->event_list->event->delta_time += delay;
  int new_time =
  num_of_bytes(get_vlq(new_track->track->event_list->event->delta_time));
  int bytes_diff = new_time - old_time;

  /* update the track length D*/

  new_track->track->length += bytes_diff;

  /* convert its instrumentation */

  remapping_t new_table;
  for (int i = 0; i <= 0xFF; i++) {
    new_table[i] = instrument;
  }
  current_event = new_track->track->event_list;
  while (current_event) {
    change_event_instrument(current_event->event, new_table);
    current_event = current_event->next_event;
  }

  /* update the number of tracks D*/

  song->num_tracks++;

  /* update the format */

  song->format = 1;

  /* find the smallest unused channel in the song */

  int channel = smallest_unused_channel(song);
  assert(channel != -1);
  current_event = new_track->track->event_list;
  while (current_event != NULL) {
    if (event_type(current_event->event) == MIDI_EVENT_T) {
      if ((current_event->event->midi_event.status >= MIN_CHANNEL_STATUS) &&
          (current_event->event->midi_event.status <= MAX_CHANNEL_STATUS)) {
        current_event->event->midi_event.status =
        (current_event->event->midi_event.status & 0xF0) + channel;
        current_event->event->type = current_event->event->midi_event.status;
      }
    }
    current_event = current_event->next_event;
  }
} /* add_round() */

/*
 * Function called prior to main that sets up random mapping tables
 */

void build_mapping_tables()
{
  for (int i = 0; i <= 0xFF; i++) {
    I_BRASS_BAND[i] = 61;
  }

  for (int i = 0; i <= 0xFF; i++) {
    I_HELICOPTER[i] = 125;
  }

  for (int i = 0; i <= 0xFF; i++) {
    N_LOWER[i] = i;
  }
  //  Swap C# for C
  for (int i = 1; i <= 0xFF; i += 12) {
    N_LOWER[i] = i-1;
  }
  //  Swap F# for G
  for (int i = 6; i <= 0xFF; i += 12) {
    N_LOWER[i] = i+1;
  }
} /* build_mapping_tables() */
