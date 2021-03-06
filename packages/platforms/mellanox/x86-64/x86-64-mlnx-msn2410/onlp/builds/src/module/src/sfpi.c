/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/
#include <onlp/platformi/sfpi.h>

#include <fcntl.h> /* For O_RDWR && open */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <onlplib/file.h>
#include <onlplib/i2c.h>
#include <onlplib/sfp.h>
#include <sys/ioctl.h>
#include "platform_lib.h"

#define MAX_SFP_PATH           64
#define SFP_SYSFS_VALUE_LEN    20
static char sfp_node_path[MAX_SFP_PATH] = {0};
#define NUM_OF_SFP_PORT        56
#define SFP_PRESENT_STATUS     "good"
#define SFP_NOT_PRESENT_STATUS "not_connected"

static int
msn2410_sfp_node_read_int(char *node_path, int *value)
{
    int data_len = 0, ret = 0;
    char buf[SFP_SYSFS_VALUE_LEN] = {0};
    *value = -1;

    ret = onlp_file_read((uint8_t*)buf, sizeof(buf), &data_len, node_path);

    if (ret == 0) {
        if (!strncmp(buf, SFP_PRESENT_STATUS, strlen(SFP_PRESENT_STATUS))) {
            *value = 1;
        } else if (!strncmp(buf, SFP_NOT_PRESENT_STATUS, strlen(SFP_NOT_PRESENT_STATUS))) {
            *value = 0;
        }
    }

    return ret;
}

static char*
msn2410_sfp_get_port_path(int port, char *node_name)
{
    sprintf(sfp_node_path, "/bsp/qsfp/qsfp%d%s", port, node_name);
    return sfp_node_path;
}

static char*
sn2410_sfp_convert_i2c_path(int port, int devaddr)
{
    sprintf(sfp_node_path, "/bsp/qsfp/qsfp%d", port);
    return sfp_node_path;
}

/************************************************************
 *
 * SFPI Entry Points
 *
 ***********************************************************/
int
onlp_sfpi_init(void)
{
    /* Called at initialization time */
    return ONLP_STATUS_OK;
}

int
onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
    /*
     * Ports {1, 32}
     */
    int p = 1;
    AIM_BITMAP_CLR_ALL(bmap);

    for (; p <= NUM_OF_SFP_PORT; p++) {
        AIM_BITMAP_SET(bmap, p);
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_is_present(int port)
{
    /*
     * Return 1 if present.
     * Return 0 if not present.
     * Return < 0 if error.
     */
    int present = -1;
    char* path = msn2410_sfp_get_port_path(port, "_status");

    if (msn2410_sfp_node_read_int(path, &present) != 0) {
        AIM_LOG_ERROR("Unable to read present status from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return present;
}

int
onlp_sfpi_presence_bitmap_get(onlp_sfp_bitmap_t* dst)
{
    int ii = 1;
    int rc = 0;

    for (;ii <= NUM_OF_SFP_PORT; ii++) {
        rc = onlp_sfpi_is_present(ii);
        AIM_BITMAP_MOD(dst, ii, (1 == rc) ? 1 : 0);
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_eeprom_read(int port, uint8_t data[256])
{
    char* path = msn2410_sfp_get_port_path(port, "");

    /*
     * Read the SFP eeprom into data[]
     *
     * Return MISSING if SFP is missing.
     * Return OK if eeprom is read
     */
    memset(data, 0, 256);

    if (onlplib_sfp_eeprom_read_file(path, data) != 0) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

int
onlp_sfpi_dev_readb(int port, uint8_t devaddr, uint8_t addr)
{
    char* path = sn2410_sfp_convert_i2c_path(port, devaddr);
    uint8_t data;
    int fd;
    int nrd;

    if (!path)
		return ONLP_STATUS_E_MISSING;

    fd = open(path, O_RDONLY);
    if (fd < 0)
		return ONLP_STATUS_E_MISSING;

    lseek(fd, addr, SEEK_SET);
    nrd = read(fd, &data, 1);
    close(fd);

    if (nrd != 1) {
		AIM_LOG_INTERNAL("Failed to read EEPROM file '%s'", path);
		return ONLP_STATUS_E_INTERNAL;
    }
    return data;
}

int
onlp_sfpi_dev_writeb(int port, uint8_t devaddr, uint8_t addr, uint8_t value)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

int
onlp_sfpi_dev_readw(int port, uint8_t devaddr, uint8_t addr)
{
    char* path = sn2410_sfp_convert_i2c_path(port, devaddr);
    uint16_t data;
    int fd;
    int nrd;

    if (!path){
		return ONLP_STATUS_E_MISSING;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
		return ONLP_STATUS_E_MISSING;
    }

    lseek(fd, addr, SEEK_SET);
    nrd = read(fd, &data, 2);
    close(fd);

    if (nrd != 2) {
		AIM_LOG_INTERNAL("Failed to read EEPROM file '%s'", path);
		return ONLP_STATUS_E_INTERNAL;
    }
    return data;
}

int
onlp_sfpi_dev_writew(int port, uint8_t devaddr, uint8_t addr, uint16_t value)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

int
onlp_sfpi_control_supported(int port, onlp_sfp_control_t control, int* rv)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

int
onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

int
onlp_sfpi_control_get(int port, onlp_sfp_control_t control, int* value)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

int
onlp_sfpi_denit(void)
{
    return ONLP_STATUS_OK;
}

