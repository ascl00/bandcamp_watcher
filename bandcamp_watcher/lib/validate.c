//
//  validate.c
//  bandcamp_watcher
//
//  Unified music folder validation for Bandcamp and Qobuz sources
//

#include "validate.h"
#include "folder.h"
#include "bandcamp.h"
#include "qobuz.h"
#include "utils.h"
#include "log.h"

int check_music_folder(const char *fullpath, const char *folder,
                       band_info_t *band_info, const char **exts, int num_exts,
                       int *source_type)
{
    if (!fullpath || !folder || !band_info || !exts || !source_type) {
        log_error("Invalid parameters to check_music_folder");
        return -1;
    }
    
    // Initialize source_type to unknown
    *source_type = SOURCE_UNKNOWN;
    
    // Step 1: Parse folder name using shared logic
    if (parse_folder_name(folder, band_info->name, sizeof(band_info->name),
                          band_info->album, sizeof(band_info->album)) != 0) {
        log_debug("Folder '%s' doesn't match Artist - Album pattern", folder);
        return -1;
    }
    
    log_debug("Parsed folder: Band='%s', Album='%s'", band_info->name, band_info->album);
    
    // Step 2: First try Bandcamp-specific validation
    // Bandcamp checks for files named "Band - Album - NN Song Name.ext"
    if (check_bandcamp_files(fullpath, band_info) == 0) {
        *source_type = SOURCE_BANDCAMP;
        log_info("Detected Bandcamp format: %s", band_info_string(band_info));
        return 0;
    }
    
    // Step 3: If Bandcamp failed, try Qobuz validation
    // Qobuz checks for sequentially numbered files (01, 02, 03...)
    if (check_qobuz_files(fullpath, band_info, exts, num_exts) == 0) {
        *source_type = SOURCE_QOBUZ;
        log_info("Detected Qobuz format: %s", band_info_string(band_info));
        return 0;
    }
    
    // Neither format matched
    log_debug("Folder '%s' doesn't match Bandcamp or Qobuz patterns", folder);
    return -1;
}
