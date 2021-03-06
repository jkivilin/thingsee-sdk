/****************************************************************************
 * drivers/input/cypress_mbr3108.c
 *
 *   Copyright (C) 2014 Haltian Ltd. All rights reserved.
 *   Author: Jussi Kivilinna <jussi.kivilinna@haltian.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/i2c.h>
#include <nuttx/kmalloc.h>

#include <nuttx/input/cypress_mbr3108.h>

#ifdef CONFIG_INPUT_CYPRESS_MBR3108_DEBUG
#  define mbr3108_dbg(x, ...)	dbg(x, ##__VA_ARGS__)
#  define mbr3108_lldbg(x, ...)	lldbg(x, ##__VA_ARGS__)
#else
#  define mbr3108_dbg(x, ...)
#  define mbr3108_lldbg(x, ...)
#endif

/****************************************************************************
 * Pre-Processor Definitions
 ****************************************************************************/

/* Register macros */

#define MBR3108_SENSOR_EN                   0x0
#define MBR3108_FSS_EN                      0x02
#define MBR3108_TOGGLE_EN                   0x04
#define MBR3108_LED_ON_EN                   0x06
#define MBR3108_SENSITIVITY0                0x08
#define MBR3108_SENSITIVITY1                0x09
#define MBR3108_BASE_THRESHOLD0             0x0C
#define MBR3108_BASE_THRESHOLD1             0x0D
#define MBR3108_FINGER_THRESHOLD2           0x0E
#define MBR3108_FINGER_THRESHOLD3           0x0F
#define MBR3108_FINGER_THRESHOLD4           0x10
#define MBR3108_FINGER_THRESHOLD5           0x11
#define MBR3108_FINGER_THRESHOLD6           0x12
#define MBR3108_FINGER_THRESHOLD7           0x13
#define MBR3108_SENSOR_DEBOUNCE             0x1C
#define MBR3108_BUTTON_HYS                  0x1D
#define MBR3108_BUTTON_LBR                  0x1F
#define MBR3108_BUTTON_NNT                  0x20
#define MBR3108_BUTTON_NT                   0x21
#define MBR3108_PROX_EN                     0x26
#define MBR3108_PROX_CFG                    0x27
#define MBR3108_PROX_CFG2                   0x28
#define MBR3108_PROX_TOUCH_TH0              0x2A
#define MBR3108_PROX_TOUCH_TH1              0x2C
#define MBR3108_PROX_RESOLUTION0            0x2E
#define MBR3108_PROX_RESOLUTION1            0x2F
#define MBR3108_PROX_HYS                    0x30
#define MBR3108_PROX_LBR                    0x32
#define MBR3108_PROX_NNT                    0x33
#define MBR3108_PROX_NT                     0x34
#define MBR3108_PROX_POSITIVE_TH0           0x35
#define MBR3108_PROX_POSITIVE_TH1           0x36
#define MBR3108_PROX_NEGATIVE_TH0           0x39
#define MBR3108_PROX_NEGATIVE_TH1           0x3A
#define MBR3108_LED_ON_TIME                 0x3D
#define MBR3108_BUZZER_CFG                  0x3E
#define MBR3108_BUZZER_ON_TIME              0x3F
#define MBR3108_GPO_CFG                     0x40
#define MBR3108_PWM_DUTYCYCLE_CFG0          0x41
#define MBR3108_PWM_DUTYCYCLE_CFG1          0x42
#define MBR3108_PWM_DUTYCYCLE_CFG2          0x43
#define MBR3108_PWM_DUTYCYCLE_CFG3          0x44
#define MBR3108_SPO_CFG                     0x4C
#define MBR3108_DEVICE_CFG0                 0x4D
#define MBR3108_DEVICE_CFG1                 0x4E
#define MBR3108_DEVICE_CFG2                 0x4F
#define MBR3108_DEVICE_CFG3                 0x50
#define MBR3108_I2C_ADDR                    0x51
#define MBR3108_REFRESH_CTRL                0x52
#define MBR3108_STATE_TIMEOUT               0x55
#define MBR3108_CONFIG_CRC                  0x7E
#define MBR3108_GPO_OUTPUT_STATE            0x80
#define MBR3108_SENSOR_ID                   0x82
#define MBR3108_CTRL_CMD                    0x86
#define MBR3108_CTRL_CMD_STATUS             0x88
#define MBR3108_CTRL_CMD_ERR                0x89
#define MBR3108_SYSTEM_STATUS               0x8A
#define MBR3108_PREV_CTRL_CMD_CODE          0x8C
#define MBR3108_FAMILY_ID                   0x8F
#define MBR3108_DEVICE_ID                   0x90
#define MBR3108_DEVICE_REV                  0x92
#define MBR3108_CALC_CRC                    0x94
#define MBR3108_TOTAL_WORKING_SNS           0x97
#define MBR3108_SNS_CP_HIGH                 0x98
#define MBR3108_SNS_VDD_SHORT               0x9A
#define MBR3108_SNS_GND_SHORT               0x9C
#define MBR3108_SNS_SNS_SHORT               0x9E
#define MBR3108_CMOD_SHIELD_TEST            0xA0
#define MBR3108_BUTTON_STAT                 0xAA
#define MBR3108_LATCHED_BUTTON_STAT         0xAC
#define MBR3108_PROX_STAT                   0xAE
#define MBR3108_LATCHED_PROX_STAT           0xAF
#define MBR3108_SYNC_COUNTER0               0xB9
#define MBR3108_DIFFERENCE_COUNT_SENSOR0    0xBA
#define MBR3108_DIFFERENCE_COUNT_SENSOR1    0xBC
#define MBR3108_DIFFERENCE_COUNT_SENSOR2    0xBE
#define MBR3108_DIFFERENCE_COUNT_SENSOR3    0xC0
#define MBR3108_DIFFERENCE_COUNT_SENSOR4    0xC2
#define MBR3108_DIFFERENCE_COUNT_SENSOR5    0xC4
#define MBR3108_DIFFERENCE_COUNT_SENSOR6    0xC6
#define MBR3108_DIFFERENCE_COUNT_SENSOR7    0xC8
#define MBR3108_GPO_DATA                    0xDA
#define MBR3108_SYNC_COUNTER1               0xDB
#define MBR3108_DEBUG_SENSOR_ID             0xDC
#define MBR3108_DEBUG_CP                    0xDD
#define MBR3108_DEBUG_DIFFERENCE_COUNT0     0xDE
#define MBR3108_DEBUG_BASELINE0             0xE0
#define MBR3108_DEBUG_RAW_COUNT0            0xE2
#define MBR3108_DEBUG_AVG_RAW_COUNT0        0xE4
#define MBR3108_SYNC_COUNTER2               0xE7

/* Device commands for MBR3108_CTRL_CMD */

#define MBR3108_CMD_COMPLETED                               0
#define MBR3108_CMD_CHECK_CONFIG_CRC                        2
#define MBR3108_CMD_SET_CONFIG_CRC                          3
#define MBR3108_CMD_ENTER_LOW_POWER_MODE                    7
#define MBR3108_CMD_CLEAR_LATCHED                           8
#define MBR3108_CMD_RESET_ADV_LOWPASS_FILTER_PROX_SENS_0    9
#define MBR3108_CMD_RESET_ADV_LOWPASS_FILTER_PROX_SENS_1    10
#define MBR3108_CMD_SOFTWARE_RESET                          255

#define MBR3108_CMD_STATUS_SUCCESS                          0
#define MBR3108_CMD_STATUS_ERROR                            1
#define MBR3108_CMD_STATUS_MASK                             1

/* Completion times for device commands */

#define MBR3108_CMD_MSECS_CHECK_CONFIG_CRC                  280 /* >220 (typ.) */
#define MBR3108_CMD_MSECS_SOFTWARE_RESET                    50
#define MBR3108_CMD_MSECS_CLEAR_LATCHED                     50

/* Other macros */

#define MBR3108_I2C_RETRIES                 10
#define MBR3108_NUM_SENSORS                 8
#define MBR3108_EXPECTED_FAMILY_ID          0x9A
#define MBR3108_EXPECTED_DEVICE_ID          0x0A03
#define MBR3108_EXPECTED_DEVICE_REV         1
#define MBR3108_SYNC_RETRIES                10

/****************************************************************************
 * Private Data Types
 ****************************************************************************/

struct mbr3108_dev_s
{
  /* I2C bus and address for device. */

  struct i2c_dev_s *i2c;
  uint8_t addr;

  /* Configuration for device. */

  struct mbr3108_board_s *board;
  const struct mbr3108_sensor_conf_s *sensor_conf;
  sem_t devsem;
  uint8_t cref;
  struct mbr3108_debug_conf_s debug_conf;
  bool int_pending;

#ifndef CONFIG_DISABLE_POLL
  struct pollfd *fds[CONFIG_INPUT_CYPRESS_MBR3108_NPOLLWAITERS];
#endif
};

/****************************************************************************
* Private Function Prototypes
*****************************************************************************/

static int mbr3108_open(FAR struct file *filep);
static int mbr3108_close(FAR struct file *filep);
static ssize_t mbr3108_read(FAR struct file *filep, FAR char *buffer,
                            size_t buflen);
static ssize_t mbr3108_write(FAR struct file *filep, FAR const char *buffer,
                             size_t buflen);
#ifndef CONFIG_DISABLE_POLL
static int mbr3108_poll(FAR struct file *filep, FAR struct pollfd *fds,
                        bool setup);
#endif

/****************************************************************************
* Private Data
****************************************************************************/

static struct mbr3108_dev_s *g_mbr3108_dev;

static const struct file_operations g_mbr3108_fileops = {
  mbr3108_open,
  mbr3108_close,
  mbr3108_read,
  mbr3108_write,
  0,
  0,
#ifndef CONFIG_DISABLE_POLL
  mbr3108_poll
#endif
};

/****************************************************************************
* Private Functions
****************************************************************************/

static int mbr3108_i2c_write(FAR struct mbr3108_dev_s *dev, uint8_t reg,
                             const uint8_t *buf, size_t buflen)
{
  struct i2c_msg_s msgv[2] =
  { {
    .addr   = dev->addr,
    .flags  = 0,
    .buffer = &reg,
    .length = 1
  }, {
    .addr   = dev->addr,
    .flags  = I2C_M_NORESTART,
    .buffer = (void *)buf,
    .length = buflen
  } };
  int ret = -EIO;
  int retries;

  /* MBR3108 will respond with NACK to address when in low-power mode. Host
   * needs to retry address selection multiple times to get MBR3108 to wake-up.
   */

  for (retries = 0; retries < MBR3108_I2C_RETRIES; retries++)
    {
      ret = I2C_TRANSFER(dev->i2c, msgv, 2);
      if (ret == -ENXIO)
        {
          /* -ENXIO is returned when getting NACK from response.
           * Keep trying. */

          continue;
        }

      if (ret >= 0)
        {
          /* Success! */

          return 0;
        }
    }

  /* Failed to read sensor. */

  return ret;
}

static int mbr3108_i2c_read(FAR struct mbr3108_dev_s *dev, uint8_t reg,
                            uint8_t *buf, size_t buflen)
{
  struct i2c_msg_s msgv[2] =
  { {
    .addr   = dev->addr,
    .flags  = 0,
    .buffer = &reg,
    .length = 1
  }, {
    .addr   = dev->addr,
    .flags  = I2C_M_READ,
    .buffer = buf,
    .length = buflen
  } };
  int ret = -EIO;
  int retries;

  /* MBR3108 will respond with NACK to address when in low-power mode. Host
   * needs to retry address selection multiple times to get MBR3108 to wake-up.
   */

  for (retries = 0; retries < MBR3108_I2C_RETRIES; retries++)
    {
      ret = I2C_TRANSFER(dev->i2c, msgv, 2);
      if (ret == -ENXIO)
        {
          /* -ENXIO is returned when getting NACK from response.
           * Keep trying.*/

          continue;
        }
      else if (ret >= 0)
        {
          /* Success! */

          return 0;
        }
      else
        {
          /* Some other error. Try to reset I2C bus and keep trying. */
#ifdef CONFIG_I2C_RESET
          if (retries == MBR3108_I2C_RETRIES - 1)
            break;

          ret = up_i2creset(dev->i2c);
          if (ret < 0)
            {
              mbr3108_dbg("up_i2creset failed: %d\n", ret);
              return ret;
            }
#endif
        }
    }

  /* Failed to read sensor. */

  return ret;
}

static int mbr3108_check_cmd_status(FAR struct mbr3108_dev_s *dev)
{
  const uint8_t start_reg = MBR3108_CTRL_CMD;
  const uint8_t last_reg = MBR3108_CTRL_CMD_ERR;
  uint8_t readbuf[MBR3108_CTRL_CMD_ERR - MBR3108_CTRL_CMD + 1];
  uint8_t cmd, cmd_status, cmd_err;
  int ret;

  DEBUGASSERT(last_reg - start_reg + 1 == sizeof(readbuf));

  /* Multi-byte read to get command status. */

  ret = mbr3108_i2c_read(dev, start_reg, readbuf, sizeof(readbuf));
  if (ret < 0)
    {
      mbr3108_dbg("cmd status get failed. ret=%d\n", ret);
      return ret;
    }

  cmd        = readbuf[MBR3108_CTRL_CMD - MBR3108_CTRL_CMD];
  cmd_status = readbuf[MBR3108_CTRL_CMD_STATUS - MBR3108_CTRL_CMD];
  cmd_err    = readbuf[MBR3108_CTRL_CMD_ERR - MBR3108_CTRL_CMD];

  mbr3108_dbg("cmd: %d, status: %d, err: %d\n", cmd, cmd_status, cmd_err);

  if (cmd != MBR3108_CMD_COMPLETED)
    {
      return -EBUSY;
    }

  if ((cmd_status & MBR3108_CMD_STATUS_MASK) == MBR3108_CMD_STATUS_SUCCESS)
    {
      /* Success. */

      return 0;
    }

  return cmd_err;
}

static int mbr3108_save_check_crc(FAR struct mbr3108_dev_s *dev)
{
  uint8_t reg = MBR3108_CTRL_CMD;
  uint8_t cmd = MBR3108_CMD_CHECK_CONFIG_CRC;
  int ret;

  ret = mbr3108_i2c_write(dev, reg, &cmd, 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108_CTRL_CMD:CHECK_CONFIG_CRC write failed.\n");
      return ret;
    }

  usleep(MBR3108_CMD_MSECS_CHECK_CONFIG_CRC * 1000);

  ret = mbr3108_check_cmd_status(dev);
  if (ret != 0)
    {
      return ret < 0 ? ret : -EIO;
    }

  return 0;
}

static int mbr3108_software_reset(FAR struct mbr3108_dev_s *dev)
{
  uint8_t reg = MBR3108_CTRL_CMD;
  uint8_t cmd = MBR3108_CMD_SOFTWARE_RESET;
  int ret;

  ret = mbr3108_i2c_write(dev, reg, &cmd, 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108_CTRL_CMD:SOFTWARE_RESET write failed.\n");
      return ret;
    }

  usleep(MBR3108_CMD_MSECS_SOFTWARE_RESET * 1000);

  ret = mbr3108_check_cmd_status(dev);
  if (ret != 0)
    {
      return ret < 0 ? ret : -EIO;
    }

  return 0;
}

static int mbr3108_enter_low_power_mode(FAR struct mbr3108_dev_s *dev)
{
  uint8_t reg = MBR3108_CTRL_CMD;
  uint8_t cmd = MBR3108_CMD_ENTER_LOW_POWER_MODE;
  int ret;

  ret = mbr3108_i2c_write(dev, reg, &cmd, 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108_CTRL_CMD:SOFTWARE_RESET write failed.\n");
      return ret;
    }

  /* Device is now in low-power mode and not scanning. Further communication
   * will cause wake-up and make chip resume scanning operations. */

  return 0;
}

static int mbr3108_clear_latched(FAR struct mbr3108_dev_s *dev)
{
  uint8_t reg = MBR3108_CTRL_CMD;
  uint8_t cmd = MBR3108_CMD_CLEAR_LATCHED;
  int ret;

  ret = mbr3108_i2c_write(dev, reg, &cmd, 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108_CTRL_CMD:MBR3108_CMD_CLEAR_LATCHED write failed.\n");
      return ret;
    }

  usleep(MBR3108_CMD_MSECS_CLEAR_LATCHED * 1000);

  ret = mbr3108_check_cmd_status(dev);
  if (ret != 0)
    {
      return ret < 0 ? ret : -EIO;
    }

  return 0;
}

static int mbr3108_debug_setup(FAR struct mbr3108_dev_s *dev,
                               FAR const struct mbr3108_debug_conf_s *conf)
{
  uint8_t reg = MBR3108_SENSOR_ID;
  int ret;

  /* Store new debug configuration. */

  dev->debug_conf = *conf;

  if (!conf->debug_mode)
    return 0;

  /* Setup debug sensor id. */

  ret = mbr3108_i2c_write(dev, reg, &conf->debug_sensor_id, 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108_SENSOR_ID write failed.\n");

      dev->debug_conf.debug_mode = false;
    }

  return ret;
}

static int mbr3108_device_configuration(FAR struct mbr3108_dev_s *dev,
                                        FAR const struct mbr3108_sensor_conf_s *conf)
{
  const uint8_t start_reg = MBR3108_SENSOR_EN;
  const uint8_t last_reg = MBR3108_CONFIG_CRC + 1;
  uint8_t value;
  int ret = 0;

  DEBUGASSERT(sizeof(conf->conf_data) == last_reg - start_reg + 1);

  ret = mbr3108_i2c_read(dev, MBR3108_CTRL_CMD, &value, 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108_CTRL_CMD read failed.\n");
      return ret;
    }

  if (value != MBR3108_CMD_COMPLETED)
    {
      /* Device is busy processing previous command. */

      return -EBUSY;
    }

  ret = mbr3108_i2c_write(dev, start_reg, conf->conf_data,
                          last_reg - start_reg + 1);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108 configuration write failed.\n");
      return ret;
    }

  ret = mbr3108_save_check_crc(dev);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108 save check CRC failed. ret=%d\n", ret);
      return ret;
    }

  ret = mbr3108_software_reset(dev);
  if (ret < 0)
    {
      mbr3108_dbg("MBR3108 software reset failed.\n");
      return ret;
    }

  dev->board->irq_enable(dev->board, true);

  return 0;
}

static int mbr3108_get_sensor_status(FAR struct mbr3108_dev_s *dev,
                                     FAR void *buf)
{
  struct mbr3108_sensor_status_s status = {};
  const uint8_t start_reg = MBR3108_BUTTON_STAT;
  const uint8_t last_reg = MBR3108_LATCHED_PROX_STAT;
  uint8_t readbuf[MBR3108_LATCHED_PROX_STAT - MBR3108_BUTTON_STAT + 1];
  int ret;

  DEBUGASSERT(last_reg - start_reg + 1 == sizeof(readbuf));

  /* Attempt to sensor status registers.*/

  ret = mbr3108_i2c_read(dev, start_reg, readbuf, sizeof(readbuf));
  if (ret < 0)
    {
      mbr3108_dbg("Sensor status read failed.\n");

      return ret;
    }

  status.button = (readbuf[MBR3108_BUTTON_STAT + 0 - start_reg]) |
                  (readbuf[MBR3108_BUTTON_STAT + 1 - start_reg] << 8);
  status.proximity = readbuf[MBR3108_PROX_STAT - start_reg];

  status.latched_button =
                  (readbuf[MBR3108_LATCHED_BUTTON_STAT + 0 - start_reg]) |
                  (readbuf[MBR3108_LATCHED_BUTTON_STAT + 1 - start_reg] << 8);
  status.latched_proximity = readbuf[MBR3108_LATCHED_PROX_STAT - start_reg];

  memcpy(buf, &status, sizeof(status));

  mbr3108_dbg("but: %x, prox: %x; latched[btn: %x, prox: %x]\n",
              status.button, status.proximity, status.latched_button,
              status.latched_button);

  return 0;
}

static int mbr3108_get_sensor_debug_data(FAR struct mbr3108_dev_s *dev,
                                         FAR void *buf)
{
  struct mbr3108_sensor_debug_s data = {};
  const uint8_t start_reg = MBR3108_SYNC_COUNTER1;
  const uint8_t last_reg = MBR3108_SYNC_COUNTER2;
  uint8_t readbuf[MBR3108_SYNC_COUNTER2 - MBR3108_SYNC_COUNTER1 + 1];
  int ret;
  int retries;
  uint8_t sync1, sync2;

  DEBUGASSERT(last_reg - start_reg + 1 == sizeof(readbuf));

  for (retries = MBR3108_SYNC_RETRIES; retries > 0; retries--)
    {
      ret = mbr3108_i2c_read(dev, start_reg, readbuf, sizeof(readbuf));
      if (ret < 0)
        {
          mbr3108_dbg("Sensor debug data read failed.\n");

          return ret;
        }

      /* Sync counters need to match. */

      sync1 = readbuf[MBR3108_SYNC_COUNTER1 - start_reg];
      sync2 = readbuf[MBR3108_SYNC_COUNTER2 - start_reg];

      if (sync1 == sync2)
        {
          break;
        }
    }

  if (retries == 0)
    {
      return -EIO;
    }

  data.sensor_average_counts =
      (readbuf[MBR3108_DEBUG_AVG_RAW_COUNT0 + 0 - start_reg]) |
      (readbuf[MBR3108_DEBUG_AVG_RAW_COUNT0 + 1 - start_reg] << 8);
  data.sensor_baseline =
      (readbuf[MBR3108_DEBUG_BASELINE0 + 0 - start_reg]) |
      (readbuf[MBR3108_DEBUG_BASELINE0 + 1 - start_reg] << 8);
  data.sensor_diff_counts =
      (readbuf[MBR3108_DEBUG_DIFFERENCE_COUNT0 + 0 - start_reg]) |
      (readbuf[MBR3108_DEBUG_DIFFERENCE_COUNT0 + 1 - start_reg] << 8);
  data.sensor_raw_counts =
      (readbuf[MBR3108_DEBUG_RAW_COUNT0 + 0 - start_reg]) |
      (readbuf[MBR3108_DEBUG_RAW_COUNT0 + 1 - start_reg] << 8);
  data.sensor_total_capacitance = readbuf[MBR3108_DEBUG_CP - start_reg];

  memcpy(buf, &data, sizeof(data));

  mbr3108_dbg("avg_cnt: %d, baseline: %d, diff_cnt: %d, raw_cnt: %d, "
              "total_cp: %d\n",
              data.sensor_average_counts, data.sensor_baseline,
              data.sensor_diff_counts, data.sensor_raw_counts,
              data.sensor_total_capacitance);

  return 0;
}

static int mbr3108_probe_device(FAR struct mbr3108_dev_s *dev)
{
  const uint8_t start_reg = MBR3108_FAMILY_ID;
  const uint8_t last_reg = MBR3108_DEVICE_REV;
  uint8_t readbuf[MBR3108_DEVICE_REV - MBR3108_FAMILY_ID + 1];
  uint8_t fam_id;
  uint16_t dev_id;
  uint8_t dev_rev;
  int ret;

  DEBUGASSERT(last_reg - start_reg + 1 == sizeof(readbuf));

  /* Attempt to read device identification registers with multi-byte read.*/

  ret = mbr3108_i2c_read(dev, start_reg, readbuf, sizeof(readbuf));
  if (ret < 0)
    {
      /* Failed to read registers from device. */

      mbr3108_dbg("Probe failed.\n");

      return ret;
    }

  /* Check result. */

  fam_id = readbuf[MBR3108_FAMILY_ID - start_reg];
  dev_id = (readbuf[MBR3108_DEVICE_ID + 0 - start_reg]) |
           (readbuf[MBR3108_DEVICE_ID + 1 - start_reg] << 8);
  dev_rev = readbuf[MBR3108_DEVICE_REV - start_reg];

  mbr3108_dbg("family_id: 0x%02x, device_id: 0x%04x, device_rev: %d\n",
              fam_id, dev_id, dev_rev);

  if (fam_id != MBR3108_EXPECTED_FAMILY_ID ||
      dev_id != MBR3108_EXPECTED_DEVICE_ID ||
      dev_rev != MBR3108_EXPECTED_DEVICE_REV)
    {
      mbr3108_dbg("Probe failed, dev-id mismatch!\n");
      mbr3108_dbg(
          "  Expected: family_id: 0x%02x, device_id: 0x%04x, device_rev: %d\n",
          MBR3108_EXPECTED_FAMILY_ID,
          MBR3108_EXPECTED_DEVICE_ID,
          MBR3108_EXPECTED_DEVICE_REV);

      return -ENXIO;
    }

  return 0;
}

static ssize_t mbr3108_read(FAR struct file *filep, FAR char *buffer,
                            size_t buflen)
{
  FAR struct inode *inode;
  FAR struct mbr3108_dev_s *priv;
  irqstate_t flags;
  int ret;

  DEBUGASSERT(filep);
  inode = filep->f_inode;

  DEBUGASSERT(inode && inode->i_private);
  priv = inode->i_private;

  ret = sem_wait(&priv->devsem);
  if (ret < 0)
    {
      return ret;
    }

  ret = -EINVAL;

  if (priv->debug_conf.debug_mode)
    {
      if (buflen == sizeof(struct mbr3108_sensor_debug_s))
        {
          ret = mbr3108_get_sensor_debug_data(priv, buffer);
        }
    }
  else
    {
      if (buflen == sizeof(struct mbr3108_sensor_status_s))
        {
          ret = mbr3108_get_sensor_status(priv, buffer);
        }
    }

  flags = irqsave();
  priv->int_pending = false;
  irqrestore(flags);

  sem_post(&priv->devsem);
  return ret < 0 ? ret : buflen;
}

static ssize_t mbr3108_write(FAR struct file *filep, FAR const char *buffer,
                             size_t buflen)
{
  FAR struct inode *inode;
  FAR struct mbr3108_dev_s *priv;
  enum mbr3108_cmd_e type;
  int ret;

  DEBUGASSERT(filep);
  inode = filep->f_inode;

  DEBUGASSERT(inode && inode->i_private);
  priv = inode->i_private;

  if (buflen < sizeof(enum mbr3108_cmd_e))
    {
      return -EINVAL;
    }

  ret = sem_wait(&priv->devsem);
  if (ret < 0)
    {
      return ret;
    }

  type = *(FAR const enum mbr3108_cmd_e *)buffer;

  switch (type)
    {
    case CYPRESS_MBR3108_CMD_SENSOR_CONF:
      {
        FAR const struct mbr3108_cmd_sensor_conf_s *conf =
            (FAR const struct mbr3108_cmd_sensor_conf_s *)buffer;

        if (buflen != sizeof(*conf))
          {
            ret = -EINVAL;
            goto out;
          }

        ret = mbr3108_device_configuration(priv, &conf->conf);
        break;
      }
    case CYPRESS_MBR3108_CMD_DEBUG_CONF:
      {
        FAR const struct mbr3108_cmd_debug_conf_s *conf =
            (FAR const struct mbr3108_cmd_debug_conf_s *)buffer;

        if (buflen != sizeof(*conf))
          {
            ret = -EINVAL;
            goto out;
          }

        ret = mbr3108_debug_setup(priv, &conf->conf);
        break;
      }
    case CYPRESS_MBR3108_CMD_CLEAR_LATCHED:
      {
        if (buflen != sizeof(type))
          {
            ret = -EINVAL;
            goto out;
          }

        ret = mbr3108_clear_latched(priv);
        break;
      }
    default:
      ret = -EINVAL;
      break;
    }

out:
  sem_post(&priv->devsem);

  return ret < 0 ? ret : buflen;
}

static int mbr3108_open(FAR struct file *filep)
{
  FAR struct inode *inode;
  FAR struct mbr3108_dev_s *priv;
  unsigned int use_count;
  int ret;

  DEBUGASSERT(filep);
  inode = filep->f_inode;

  DEBUGASSERT(inode && inode->i_private);
  priv = inode->i_private;

  while (sem_wait(&priv->devsem) != 0)
    {
      assert(errno == EINTR);
    }

  use_count = priv->cref + 1;
  if (use_count == 1)
    {
      /* First user, do power on. */

      ret = priv->board->set_power(priv->board, true);
      if (ret < 0)
        {
          goto out_sem;
        }

      /* Let chip to power up before probing */

      usleep(100 * 1000);

      /* Check that device exists on I2C. */

      ret = mbr3108_probe_device(priv);
      if (ret < 0)
        {
          /* No such device. Power off the switch. */

          (void)priv->board->set_power(priv->board, false);
          goto out_sem;
        }

      if (priv->sensor_conf)
        {
          /* Do configuration. */

          ret = mbr3108_device_configuration(priv, priv->sensor_conf);
          if (ret < 0)
            {
              /* Configuration failed. Power off the switch. */

              (void)priv->board->set_power(priv->board, false);
              goto out_sem;
            }
        }

      priv->cref = use_count;
    }
  else
    {
      DEBUGASSERT(use_count < UINT8_MAX && use_count > priv->cref);

      priv->cref = use_count;
      ret = 0;
    }

out_sem:
  sem_post(&priv->devsem);
  return ret;
}

static int mbr3108_close(FAR struct file *filep)
{
  FAR struct inode *inode;
  FAR struct mbr3108_dev_s *priv;
  int use_count;

  DEBUGASSERT(filep);
  inode = filep->f_inode;

  DEBUGASSERT(inode && inode->i_private);
  priv = inode->i_private;

  while (sem_wait(&priv->devsem) != 0)
    {
      assert(errno == EINTR);
    }

  use_count = priv->cref - 1;
  if (use_count == 0)
    {
      /* Disable interrupt */

      priv->board->irq_enable(priv->board, false);

      /* Set chip in low-power mode. */

      (void)mbr3108_enter_low_power_mode(priv);

      /* Last user, do power off. */

      (void)priv->board->set_power(priv->board, false);

      priv->debug_conf.debug_mode = false;
      priv->cref = use_count;
    }
  else
    {
      DEBUGASSERT(use_count > 0);

      priv->cref = use_count;
    }

  sem_post(&priv->devsem);

  return 0;
}

#ifndef CONFIG_DISABLE_POLL

static void mbr3108_poll_notify(FAR struct mbr3108_dev_s *priv)
{
  int i;

  DEBUGASSERT(priv != NULL);

  for (i = 0; i < CONFIG_INPUT_CYPRESS_MBR3108_NPOLLWAITERS; i++)
    {
      struct pollfd *fds = priv->fds[i];
      if (fds)
        {
          mbr3108_lldbg("Report events: %02x\n", fds->revents);

          fds->revents |= POLLIN;
          sem_post(fds->sem);
        }
    }
}

static int mbr3108_poll(FAR struct file *filep, FAR struct pollfd *fds,
                        bool setup)
{
  FAR struct mbr3108_dev_s *priv;
  FAR struct inode *inode;
  bool pending;
  int ret = 0;
  int i;

  DEBUGASSERT(filep && fds);
  inode = filep->f_inode;

  DEBUGASSERT(inode && inode->i_private);
  priv = (FAR struct mbr3108_dev_s *)inode->i_private;

  ret = sem_wait(&priv->devsem);
  if (ret < 0)
    {
      return ret;
    }

  if (setup)
    {
      /* Ignore waits that do not include POLLIN */

      if ((fds->events & POLLIN) == 0)
        {
          ret = -EDEADLK;
          goto out;
        }

      /* This is a request to set up the poll.  Find an available slot for the
       * poll structure reference */

      for (i = 0; i < CONFIG_INPUT_CYPRESS_MBR3108_NPOLLWAITERS; i++)
        {
          /* Find an available slot */

          if (!priv->fds[i])
            {
              /* Bind the poll structure and this slot */

              priv->fds[i] = fds;
              fds->priv = &priv->fds[i];
              break;
            }
        }

      if (i >= CONFIG_INPUT_CYPRESS_MBR3108_NPOLLWAITERS)
        {
          fds->priv = NULL;
          ret = -EBUSY;
        }
      else
        {
          pending = priv->int_pending;
          if (pending)
            {
              mbr3108_poll_notify(priv);
            }
        }
    }
  else if (fds->priv)
    {
      /* This is a request to tear down the poll. */

      struct pollfd **slot = (struct pollfd **)fds->priv;
      DEBUGASSERT(slot != NULL);

      /* Remove all memory of the poll setup */

      *slot = NULL;
      fds->priv = NULL;
    }

out:
  sem_post(&priv->devsem);
  return ret;
}

#endif /* !CONFIG_DISABLE_POLL */

static int mbr3108_isr_handler(int irq, FAR void *context)
{
  if (!g_mbr3108_dev)
    return 0;

  g_mbr3108_dev->int_pending = true;

#ifndef CONFIG_DISABLE_POLL
  mbr3108_poll_notify(g_mbr3108_dev);
#endif

  mbr3108_lldbg("int!\n");

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int cypress_mbr3108_register(FAR const char *devpath,
                             FAR struct i2c_dev_s *i2c_bus,
                             uint8_t i2c_devaddr,
                             struct mbr3108_board_s *board_config,
                             const struct mbr3108_sensor_conf_s *sensor_conf)
{
  struct mbr3108_dev_s *priv;
  int ret = 0;

  /* Allocate device private structure. */

  g_mbr3108_dev = kmm_zalloc(sizeof(struct mbr3108_dev_s));
  if (!g_mbr3108_dev)
    {
      mbr3108_dbg("Memory cannot be allocated for mbr3108 sensor\n");
      return -ENOMEM;
    }

  /* Setup device structure. */

  priv = g_mbr3108_dev;
  priv->addr = i2c_devaddr;
  priv->i2c = i2c_bus;
  priv->board = board_config;
  priv->sensor_conf = sensor_conf;

  sem_init(&priv->devsem, 0, 1);

  ret = register_driver(devpath, &g_mbr3108_fileops, 0666, priv);
  if (ret < 0)
    {
      kmm_free(g_mbr3108_dev);
      g_mbr3108_dev = NULL;
      mbr3108_dbg("Error occurred during the driver registering\n");
      return ret;
    }

  mbr3108_dbg("Registered with %d\n", ret);

  /* Prepare interrupt line and handler. */

  priv->board->irq_clear(priv->board);
  priv->board->irq_attach(priv->board, mbr3108_isr_handler);
  priv->board->irq_enable(priv->board, false);

  return 0;
}
