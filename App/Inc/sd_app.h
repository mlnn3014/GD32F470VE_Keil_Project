#ifndef __SD_APP_H_
#define __SD_APP_H_

#include <stdint.h>

#ifndef SD_FATFS_DEMO_ENABLE
#define SD_FATFS_DEMO_ENABLE (0U)
#endif

int sd_app_init(void);
int sd_mount(void);
uint8_t sd_is_mounted(void);
int sd_write_file(const char *path, const void *data, uint32_t len);
int sd_append_file(const char *path, const void *data, uint32_t len);
int sd_read_file(const char *path, void *data, uint32_t size, uint32_t *read_len);
void sd_print_info(void);
int sd_self_test(void);

#endif /* __SD_APP_H_ */
