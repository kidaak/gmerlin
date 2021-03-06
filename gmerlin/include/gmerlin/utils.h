/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#ifndef __BG_UTILS_H_
#define __BG_UTILS_H_

#include <stdio.h>

#include <gavl/gavl.h>
#include <gavl/metadata.h>
#include <gavl/utils.h>

/** \defgroup utils Utilities
 *  \brief Utility functions
 *
 *  These are lots of utility functions, which should be used, whereever
 *  possible, instead of their libc counterparts (if any). That's because
 *  they also handle portability issues, ans it's easier to make one single
 *  function portable (with \#ifdefs if necessary) and use it throughout the code.
 */

/** \defgroup files Filesystem support
 *  \ingroup utils
 *  \brief Functions for files and directories
 *
 *  @{
 */

/** \brief Append a trailing slash to a path name
 *  \param path Old path (will eventually be freed).
 *  \returns The path, which is garantueed to end with a '/'
 */

/* Append a trailing '/' if it's missing. Argument must be free()able */

char * bg_fix_path(char * path);

/** \brief Ensure that a directory exists
 *  \param dir Directory
 *  \returns 1 if the directory exists after the function call, 0 else
 *
 *  Non-existing directories will be created if possible
 */

int bg_ensure_directory(const char * dir);


/** \brief Search for a file for reading
 *  \param directory Directory
 *  \param file Filename
 *  \returns A filename or NULL
 *
 *  This function first seeks in the system gmerlin data directory
 *  (e.g. /usr/local/share/gmerlin), then in $HOME/.gmerlin for the
 *  specified file, which must be readable. The directory can also contain
 *  subdirectories e.g. "player/tree".
 */

char * bg_search_file_read(const char * directory, const char * file);

/** \brief Search for a file for writing
 *  \param directory Directory
 *  \param file Filename
 *  \returns A filename or NULL
 *
 *  This function first seeks in the in $HOME/.gmerlin for the
 *  specified file, which must be writable. If the file doesn't
 *  exist, an empty file is created. If the directory doesn't exist,
 *  it's created as well. The directory can also contain
 *  subdirectories e.g. "player/tree".
 */

char * bg_search_file_write(const char * directory, const char * file);

/** \brief Search for an executable
 *  \param file Name of the file (without dirtectory)
 *  \param path If non NULL, the complete path to the exectuable will be returned
 *  \returns 1 if executeable is found anywhere in $PATH:/opt/gmerlin/bin, 0 else.
 *
 *  If path is non NULL, it will contain the path to the executable,
 *  which must be freed after
 */

int bg_search_file_exec(const char * file, char ** path);

/** \brief Find an URL launcher
 *  \returns A newly allocated string, which must be freed
 *
 * This returnes the path of a webbrowser. Under gnome, it will be the
 * your default webbrowser, under other systems, this function will try a
 * list of known webbrowsers.
 */
 
char * bg_find_url_launcher();

/** \brief Display html help
 *  \param path Path
 *
 *  Launch a webbrowser and display a html file.
 *  Path is something lile "userguide/Player.html"
 */

void bg_display_html_help(const char * path);

/** \brief Create a unique filename.
 *  \param format Printf like format. Must contain "%08x" as the only placeholder.
 *  \returns A newly allocated string
 *
 *  Create a unique filename, and create an empty file of this name.
 */

char * bg_create_unique_filename(char * format);

/** \brief Get the canonical filename
 *  \param name Filename
 *  \returns A newly allocated filename
 *
 *  On Glibc systems, this calls canonicalize_file_name(3)
 */

char * bg_canonical_filename(const char * name);

/** \brief Ensure a file extension
 *  \param filename Filename (with or without extension)
 *  \param ext Extension to look for
 *  \returns A newly allocated filename
 *
 *  This function checks case insensitively if filename
 *  ends with the extension. If yes, it returns a copy of
 *  the filename. If not, it returns a copy with the extension
 *  appended. The extention must not start with the dot.
 */

char * bg_filename_ensure_extension(const char * filename,
                                    const char * ext);



/** @} */

/** \defgroup strings String utilities
 *  \ingroup utils
 *  \brief String utilities
 *
 * @{
 */

/** \brief Duplicate a string
 *  \param old_string (will eventually be freed)
 *  \param new_string New string
 *
 *  Both old_string and new_string can be NULL. If
 *  new string is the empty string (""), if will be treated like
 *  NULL.
 */

// char * bg_strdup(char * old_string, const char * new_string);

/** \brief Duplicate a string from a part of a source string
 *  \param old_string (will eventually be freed)
 *  \param new_start Start of the new string
 *  \param new_end Points to the first character after the end of the new string
 *
 *  Any combination of old_string, new_start and new_end can be NULL.
 *  If new_start is the empty string (""), if will be treated like
 *  NULL. If new_end is NULL, it will be obtained with strlen().
 */

// char * bg_strndup(char * old_string,
//                  const char * new_start,
//                  const char * new_end);

/** \brief Concatenate two strings
 *  \param old_string Old string (will be freed)
 *  \param tail Will be appended to old_string
 *  \returns A newly allocated string.
 */

// char * bg_strcat(char * old_string, const char * tail);

/** \brief Append a part of a string to another string
 *  \param old_string Old string (will be freed)
 *  \param start Start of the string to be appended
 *  \param end Points to the first character after the end of the string to be appended
 *  \returns A newly allocated string.
 */

// char * bg_strncat(char * old_string, const char * start, const char * end);

/** \brief Convert an UTF-8 string to uppercase
 *  \param str String
 *  \returns A newly allocated string.
 */

char * bg_toupper(const char * str);


/** \brief Check if a string looks like an URL.
 *  \param str A string
 *  \returns 1 if the string looks like an URL, 0 else
 *
 *  This checks mostly for the occurrence of "://" at a location, where it makes
 *  sense.
 */

int bg_string_is_url(const char * str);

/** \brief Split an URL into their parts
 *  \param url An URL
 *  \param protocol Protocol (returned)
 *  \param user Username (returned)
 *  \param password Password (returned)
 *  \param hostname Hostname (returned)
 *  \param port     Port (returned)
 *  \param path     Path (returned)
 *
 * This parses an url in the form
 * \<protocol\>://\<user\@password\>\<host\>\<:port\>\<path\>. All arguments
 * for returning the path can be NULL. port will be set to -1 if if doesn't
 * occur in the URL. All strings must be freed when non-NULL after the call.
 */

int bg_url_split(const char * url,
                 char ** protocol,
                 char ** user,
                 char ** password,
                 char ** hostname,
                 int * port,
                 char ** path);

/*
 *  \brief Split off URL variables
 *  \param path Path component of an URL
 *  \param vars Place to store the variables
 *
 *  This will split off variables
 *  like path?var1=val1&var2=val2 and store them into the
 *  metadata structure. 
 *  
 */
 
void bg_url_get_vars(char * path,
                     gavl_metadata_t * vars);


/** \brief Get MD5 hash of a filename
 *  \param gml
 *  \param ret Returns the MD5 sum
 *
 *  This creates an MD5 hash of a gml. For regular
 *  files this is compatible with the thumbnailing
 *  specification
 */

void bg_get_filename_hash(const char * gml, char ret[33]);

/** \brief Print into a string
 *  \param format printf like format
 *
 *  All other arguments must match the format like in printf.
 *  This function allocates the returned string, thus it must be
 *  freed by the caller.
 */

char * bg_sprintf(const char * format,...) __attribute__ ((format (printf, 1, 2)));

/** \brief Break a string into substrings
 *  \param str String
 *  \param delim Delimiter for the substrings
 *  \returns A NULL terminated array of substrings
 *
 *  Free the result with \ref bg_strbreak_free.
 */

char ** bg_strbreak(const char * str, char delim);

/** \brief Free a substrings array
 *  \param retval Array
 *
 *  Use this for substring arrays returned by \ref bg_strbreak.
 */

void bg_strbreak_free(char ** retval);

/** \brief Scramble a string
 *  \param str String to be scrambled
 *  \returns A newly allocated scrambled string
 *
 * Note:
 * Don't even think about using this for security sensitive stuff.
 * It's for saving passwords in files, which should be readable by the
 * the owner only.
 */

char * bg_scramble_string(const char * str);

/** \brief Descramble a string
 *  \param str String to be descrambled
 *  \returns A newly allocated descrambled string
 *
 * Note:
 * Don't even think about using this for security sensitive stuff.
 * It's for saving passwords in files, which should be readable by the
 * the owner only.
 */

char * bg_descramble_string(const char * str);

/** \brief Convert a binary string (in system charset) to an URI
 *  \param pos1 The string
 *  \param len or -1
 *
 * This e.g. replaces " " by "%20".
 */

char * bg_string_to_uri(const char * pos1, int len);

/** \brief Convert an URI to a a binary string (in system charset)
 *  \param pos1 The string
 *  \param len or -1
 *
 * This e.g. replaces "%20" by " ".
 */

char * bg_uri_to_string(const char * pos1, int len);

/** \brief Decode an  URI list
 *  \param str String
 *  \param len Length of the string or -1
 *
 *  This one decodes a string of MIME type text/urilist into
 *  a gmerlin usable array of location strings.
 *  The returned array is NULL terminated, it must be freed by the
 *  caller with bg_urilist_free.
 */

char ** bg_urilist_decode(const char * str, int len);

/** \brief Free an URI list
 *  \param uri_list Decoded URI list returned by \ref bg_uri_to_string
 */

void bg_urilist_free(char ** uri_list);

/** \brief Convert a string from the system character set to UTF-8
 *  \param str String
 *  \param len Length or -1
 *  \returns A newly allocated string
 *
 *  The "system charset" is obtained with nl_langinfo().
 */

char * bg_system_to_utf8(const char * str, int len);

/** \brief Convert a string from UTF-8 to the system character set
 *  \param str String
 *  \param len Length or -1
 *  \returns A newly allocated string
 *
 *  The "system charset" is obtained with nl_langinfo().
 */

char * bg_utf8_to_system(const char * str, int len);

/** \brief Get a language name
 *  \param iso An iso-639 3 character code
 *  \returns The name of the language or NULL
 */

const char * bg_get_language_name(const char * iso);

/** \brief Check if a string occurs in a space-separated list of strings
 *  \param str String
 *  \param key_list Space separated list of keys
 *  \returns 1 of str occurs in key_list, 0 else
 */

int bg_string_match(const char * str, const char * key_list);

/* @} */

/** \defgroup misc Misc stuff
 *  \ingroup utils
 *
 *  @{
 */


/** \brief Do a hexdump of binary data
 *  \param data Data
 *  \param len Length
 *  \param linebreak How many bytes to print in each line before a linebreak
 *
 *  This is mostly for debugging
 */

void bg_hexdump(uint8_t * data, int len, int linebreak);

/** \brief Convert an audio format to a string
 *  \param format An audio format
 *  \param use_tabs 1 to use tabs for separating field names and values
 *  \returns A newly allocated string
 */

char * bg_audio_format_to_string(gavl_audio_format_t * format, int use_tabs);


/** \brief Convert a video format to a string
 *  \param format A video format
 *  \param use_tabs 1 to use tabs for separating field names and values
 *  \returns A newly allocated string
 */

char * bg_video_format_to_string(gavl_video_format_t * format, int use_tabs);

/** \brief Create a stream label
 *  \param index Index of the stream (starting with 0)
 *  \param m Metadata
 *  \returns A newly allocated string
 */

char * bg_get_stream_label(int index, const gavl_metadata_t * m);

/** \brief Dump to stderr
 *  \param format Format (printf compatible)
 */

void bg_dprintf(const char * format, ...) __attribute__ ((format (printf, 1, 2)));

/** \brief Dump to stderr with intendation
 *  \param indent How many spaces to prepend
 *  \param format Format (printf compatible)
 */

void bg_diprintf(int indent, const char * format, ...) __attribute__ ((format (printf, 2, 3)));

/** \brief Read an entire file into a buffer
 *  \param filename Name of the file
 *  \param len Returns the length in bytes
 *  \returns A buffer containing the entire file
 */

void * bg_read_file(const char * filename, int * len);

/** \brief Write an entire file into a buffer
 *  \param filename Name of the file
 *  \param data Data to write
 *  \param len Length in bytes
 *  \returns 1 on success, 0 on failure
 */

int bg_write_file(const char * filename, void * data, int len);

/** \brief Lock a file for exclusive access
 *  \param f An open file
 *  \param wr An open file
 *  \returns 1 on success, 0 on failure
 */

int bg_lock_file(FILE * f, int wr);

/** \brief Unlock a file for exclusive access
 *  \param f An open file
 *  \returns 1 on success, 0 on failure
 */

int bg_unlock_file(FILE * f);

/** \brief Get the size of an open file
 *  \param f A file opened for reading
 *  \returns File size in bytes
 */

size_t bg_file_size(FILE * f);

/** \brief Convert a ISO 639-2/B language code to a ISO 639-2/T code
 *  \param code ISO 639-2/B
 *  \returns ISO 639-2/T code
 */

const char * bg_iso639_b_to_t(const char * code);

/** \brief Set a date and time field of the metadata from the lcoal time
 *  \param m Metadata
 *  \param field Field to set
 */

void bg_metadata_date_now(gavl_metadata_t * m, const char * key);

/** \brief Make the current process become a daemon
 */

void bg_daemonize();


/* @} */

extern char const * const bg_language_codes[];
extern char const * const bg_language_labels[];

#ifdef DEBUG
#define bg_debug(f,...) fprintf(stderr, f, __VA_ARGS__)
#else
#define bg_debug(f,...)
#endif



#endif // __BG_UTILS_H_
