/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1752778
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/util/lock.h>
#include <arch/x86/pio.h>

using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::util;
using namespace infos::arch::x86;

class CMOSRTC : public RTC
{
    public:
      static const DeviceClass CMOSRTCDeviceClass;

      const DeviceClass &device_class() const override
      {
            return CMOSRTCDeviceClass;
      }

      // void out_byte(int port, int value);
      // int in_byte(int port);

      enum
      {
            cmos_address = 0x70,
            cmos_data = 0x71
      };

      int get_update_in_progress_flag()
      {
            __outb(cmos_address, 0xA);
            return (__inb(cmos_data) & 0x80);
      }

      unsigned char get_RTC_register(int reg)
      {
            __outb(cmos_address, reg);
            return __inb(cmos_data);
      }

      bool tp_eq(RTCTimePoint a, RTCTimePoint b)
      {
            return (
                (a.seconds == b.seconds) &&
                (a.minutes == b.minutes) &&
                (a.hours == b.hours) &&
                (a.day_of_month == b.day_of_month) &&
                (a.month == b.month) &&
                (a.year == b.year) &&
                true);
      }

      /**
	 * Interrogates the RTC to read the current date & time.
	 * @param tp Populates the tp structure with the current data & time, as
	 * given by the CMOS RTC device.
	 */
      void read_timepoint(RTCTimePoint &tp) override
      {
            // FILL IN THIS METHOD - WRITE HELPER METHODS IF NECESSARY
            unsigned char registerB;
            unsigned char second;
            unsigned char minute;
            unsigned char hour;
            unsigned char day;
            unsigned char month;
            unsigned int year;

            UniqueIRQLock l;

            // Note: This uses the "read registers until you get the same values twice in a row" technique
            //       to avoid getting dodgy/inconsistent values due to RTC updates

            while (get_update_in_progress_flag()); // Make sure an update isn't in progress
            second = get_RTC_register(0x00);
            minute = get_RTC_register(0x02);
            hour = get_RTC_register(0x04);
            day = get_RTC_register(0x07);
            month = get_RTC_register(0x08);
            year = get_RTC_register(0x09);

            tp = RTCTimePoint{
                .seconds = second,
                .minute = minute,
                .hours = hour,
                .day_of_month = day,
                .month = month,
                .year = year,
                };

            RTCTimePoint previous = tp;

            do
            {
                  previous = tp;

                  while (get_update_in_progress_flag())
                        ; // Make sure an update isn't in progress
                  second = get_RTC_register(0x00);
                  minute = get_RTC_register(0x02);
                  hour = get_RTC_register(0x04);
                  day = get_RTC_register(0x07);
                  month = get_RTC_register(0x08);
                  year = get_RTC_register(0x09);
            } while (!tp_eq(now, previous));

            registerB = get_RTC_register(0xB);

            // Convert BCD to binary values if necessary

            if (!(registerB & 0x04))
            {
                  second = (second & 0x0F) + ((second / 16) * 10);
                  minute = (minute & 0x0F) + ((minute / 16) * 10);
                  hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
                  day = (day & 0x0F) + ((day / 16) * 10);
                  month = (month & 0x0F) + ((month / 16) * 10);
                  year = (year & 0x0F) + ((year / 16) * 10);
            }

            // Convert 12 hour clock to 24 hour clock if necessary

            if (!(registerB & 0x02) && (hour & 0x80))
            {
                  hour = ((hour & 0x7F) + 12) % 24;
            }

            // Calculate the full (4-digit) year

            year += (tp.year / 100) * 100;
            if (year < tp.year)
                  year += 100;
      }

      // private:
      // 	void out_byte(int port, int value);
      // 	int in_byte(int port);

      // 	enum {
      // 		cmos_address = 0x70,
      // 		cmos_data    = 0x71
      // 	};

      // 	int get_update_in_progress_flag() {
      // 		out_byte(cmos_address, 0x0A);
      // 		return (in_byte(cmos_data) & 0x80);
      // 	}

      // 	unsigned char get_RTC_register(int reg) {
      // 		out_byte(cmos_address, reg);
      // 		return in_byte(cmos_data);
      // 	}

      // 	RTCTimePoint get_timepoint() {
      // 		while (get_update_in_progress_flag()) {
      // 			// If flag is hoisted
      // 			// Do not dwell on reasons why
      // 			// Await emptiness
      // 		}

      // 	}
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
