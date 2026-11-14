#include "asciiview.h"

#include <QString>
#include <cstring>

#if defined(Q_OS_LINUX)
#include <scsi/sg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(Q_OS_MACOS)
#include <fcntl.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/ata/IOATAStorageDefines.h>
#include <IOKit/storage/ata/ATASMARTLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(Q_OS_LINUX)

QVector<unsigned char> AsciiView::readSMARTData(const QString& device_path) {
    int fd;
    unsigned char smart_read_cmd[SMART_READ_CMD_LEN] = {0xa1, 0x0c, 0x0e, 0xd0, 1, 0, 0x4f, 0xc2, 0, 0xb0, 0, 0};
    unsigned char inquiry_cmd[INQUIRY_CMD_LEN] = {0x12, 0, 0, 0, INQUIRY_RESP_LEN, 0};
    unsigned char smart_read_resp[SMART_READ_RESP_LEN];
    unsigned char inquiry_resp[INQUIRY_RESP_LEN];
    unsigned char sense_buffer[SENSE_BUFFER_LEN];
    sg_io_hdr_t io_hdr;
    QVector<unsigned char> result;

    fd = open(device_path.toStdString().c_str(), O_RDONLY);
    if (fd < 0) {
        return result;
    }

    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = SMART_READ_CMD_LEN;
    io_hdr.mx_sb_len = SENSE_BUFFER_LEN;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = SMART_READ_RESP_LEN;
    io_hdr.dxferp = smart_read_resp;
    io_hdr.cmdp = smart_read_cmd;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 20000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        close(fd);
        return result;
    }

    result.resize(SMART_READ_RESP_LEN);
    memcpy(result.data(), smart_read_resp, SMART_READ_RESP_LEN);

    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = INQUIRY_CMD_LEN;
    io_hdr.mx_sb_len = SENSE_BUFFER_LEN;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = INQUIRY_RESP_LEN;
    io_hdr.dxferp = inquiry_resp;
    io_hdr.cmdp = inquiry_cmd;
    io_hdr.sbp = sense_buffer;
    io_hdr.timeout = 20000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        close(fd);
        return result;
    }

    qsizetype currentSize = result.size();
    result.resize(static_cast<int>(currentSize + INQUIRY_RESP_LEN));
    memcpy(result.data() + currentSize, inquiry_resp, INQUIRY_RESP_LEN);

    close(fd);
    return result;
}

#elif defined(Q_OS_MACOS)

// macOS has no SG_IO ioctl. SMART data is read through the IOKit user clients that
// smartctl itself uses; the interface definitions below are adapted from
// smartmontools' os_darwin.cpp/os_darwin.h (GPL-2.0-or-later).
namespace {

// kIOMainPortDefault was introduced in the macOS 12 SDK.
#if !defined(kIOMainPortDefault)
#if defined(kIOMasterPortDefault)
#define kIOMainPortDefault kIOMasterPortDefault
#else
#define kIOMainPortDefault 0
#endif
#endif

#define QDISKINFO_IOATABLOCKSTORAGEDEVICE_CLASS "IOATABlockStorageDevice"

#define kIONVMeSMARTUserClientTypeID CFUUIDGetConstantUUIDWithBytes(NULL, \
    0xAA, 0x0F, 0xA6, 0xF9, 0xC2, 0xD6, 0x45, 0x7F, \
    0xB1, 0x0B, 0x59, 0xA1, 0x32, 0x53, 0x29, 0x2F)

#define kIONVMeSMARTInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL, \
    0xcc, 0xd1, 0xdb, 0x19, 0xfd, 0x9a, 0x4d, 0xaf, \
    0xbf, 0x95, 0x12, 0x45, 0x4b, 0x23, 0x0a, 0xb6)

// Based on the interface exposed by the AppleNVMeSMART plugin.
typedef struct IONVMeSMARTInterface
{
    IUNKNOWN_C_GUTS;

    UInt16 version;
    UInt16 revision;

    IOReturn (*SMARTReadData)(void *interface, void *nvmeSmartData);
    IOReturn (*GetIdentifyData)(void *interface, void *nvmeIdentifyController, unsigned int ns);

    UInt64 reserved0;
    UInt64 reserved1;

    IOReturn (*GetLogPage)(void *interface, void *data, unsigned int logPageId, unsigned int numDWords);

    UInt64 reserved2;
    UInt64 reserved3;
    UInt64 reserved4;
    UInt64 reserved5;
    UInt64 reserved6;
    UInt64 reserved7;
    UInt64 reserved8;
    UInt64 reserved9;
    UInt64 reserved10;
    UInt64 reserved11;
    UInt64 reserved12;
    UInt64 reserved13;
    UInt64 reserved14;
    UInt64 reserved15;
    UInt64 reserved16;
    UInt64 reserved17;
    UInt64 reserved18;
    UInt64 reserved19;
} IONVMeSMARTInterface;

// Returns true when the registry entry exposes SMART data for the given protocol.
bool isSmartCapable(io_object_t device, bool checkNvme)
{
    CFTypeRef capableKey = IORegistryEntryCreateCFProperty(
        device,
        checkNvme ? CFSTR("NVMe SMART Capable") : CFSTR("SMART Capable"),
        kCFAllocatorDefault, 0);

    if (capableKey) {
        CFRelease(capableKey);
        return true;
    }

    if (!checkNvme && IOObjectConformsTo(device, QDISKINFO_IOATABLOCKSTORAGEDEVICE_CLASS)) {
        CFDictionaryRef characteristics = static_cast<CFDictionaryRef>(
            IORegistryEntryCreateCFProperty(device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
                                            kCFAllocatorDefault, kNilOptions));
        if (!characteristics) {
            return false;
        }

        CFNumberRef features = nullptr;
        UInt32 ataFeatures = 0;
        if (CFDictionaryGetValueIfPresent(characteristics, CFSTR("ATA Features"),
                                          reinterpret_cast<const void **>(&features)) && features) {
            CFNumberGetValue(features, kCFNumberLongType, &ataFeatures);
        }
        CFRelease(characteristics);

        return (ataFeatures & kIOATAFeatureSMART) != 0;
    }

    return false;
}

// Walks up the IOService plane until an entry that supports SMART is found.
// The returned object belongs to the caller and must be released with IOObjectRelease().
io_object_t findSmartService(io_object_t start, bool checkNvme)
{
    io_object_t current = start;
    if (!current) {
        return 0;
    }
    IOObjectRetain(current);

    while (!isSmartCapable(current, checkNvme)) {
        io_object_t parent = 0;
        const IOReturn error = IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent);
        IOObjectRelease(current);
        if (error != kIOReturnSuccess || !parent) {
            return 0;
        }
        current = parent;
    }

    return current;
}

bool readAtaSmartData(io_object_t service, QVector<unsigned char> &result)
{
    IOCFPlugInInterface **plugin = nullptr;
    IOATASMARTInterface **smartInterface = nullptr;
    SInt32 score = 0;

    if (IOCreatePlugInInterfaceForService(service, kIOATASMARTUserClientTypeID, kIOCFPlugInInterfaceID,
                                         &plugin, &score) != kIOReturnSuccess || !plugin) {
        return false;
    }

    bool success = false;
    if ((*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOATASMARTInterfaceID),
                                 reinterpret_cast<LPVOID *>(&smartInterface)) == S_OK && smartInterface) {
        result.resize(SMART_READ_RESP_LEN);
        success = (*smartInterface)->SMARTReadData(smartInterface,
                                                  reinterpret_cast<ATASMARTData *>(result.data())) == kIOReturnSuccess;
    }

    // The interfaces are intentionally not released: this code runs in a short lived
    // child process that exits right after the data has been written out.
    if (!success) {
        result.clear();
    }
    return success;
}

bool readNvmeSmartData(io_object_t service, QVector<unsigned char> &result)
{
    IOCFPlugInInterface **plugin = nullptr;
    IONVMeSMARTInterface **smartInterface = nullptr;
    SInt32 score = 0;

    if (IOCreatePlugInInterfaceForService(service, kIONVMeSMARTUserClientTypeID, kIOCFPlugInInterfaceID,
                                         &plugin, &score) != kIOReturnSuccess || !plugin) {
        return false;
    }

    bool success = false;
    if ((*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIONVMeSMARTInterfaceID),
                                 reinterpret_cast<LPVOID *>(&smartInterface)) == S_OK && smartInterface) {
        // SMART / Health Information log page (0x02), 128 dwords = 512 bytes.
        result.resize(SMART_READ_RESP_LEN);
        success = (*smartInterface)->GetLogPage(smartInterface, result.data(), 0x02, 127) == kIOReturnSuccess;
    }

    if (!success) {
        result.clear();
    }
    return success;
}

} // namespace

QVector<unsigned char> AsciiView::readSMARTData(const QString& device_path) {
    QVector<unsigned char> result;

    // Accepts /dev/disk0, /dev/rdisk0 and disk0, matching smartctl.
    const QByteArray bsdName = device_path.section(QLatin1Char('/'), -1).toUtf8();

    CFMutableDictionaryRef matcher = IOBSDNameMatching(kIOMainPortDefault, 0, bsdName.constData());
    if (!matcher) {
        return result;
    }

    const io_service_t media = IOServiceGetMatchingService(kIOMainPortDefault, matcher);
    if (!media) {
        return result;
    }

    io_object_t ataService = findSmartService(media, false);
    io_object_t nvmeService = ataService ? 0 : findSmartService(media, true);
    IOObjectRelease(media);

    if (ataService) {
        readAtaSmartData(ataService, result);
        IOObjectRelease(ataService);
    } else if (nvmeService) {
        readNvmeSmartData(nvmeService, result);
        IOObjectRelease(nvmeService);
    }

    // Only the 512 byte SMART page is available here; macOS has no SCSI INQUIRY
    // passthrough, so the trailing INQUIRY block of the Linux output is omitted.
    return result;
}

#else

QVector<unsigned char> AsciiView::readSMARTData(const QString&) {
    return QVector<unsigned char>();
}

#endif

QString AsciiView::hexDump(const QVector<unsigned char> &data) {
    QString result;
    qsizetype dataSize = data.size();
    int offset = 0;

    for (qsizetype i = 0; i < dataSize; i += 16) {
        QString line;
        line += QString("%1  ").arg(offset, 8, 16, QChar('0')).toUpper();
        offset += 16;
        for (qsizetype j = 0; j < 16; ++j) {
            if (i + j < dataSize) {
                line += QString("%1 ")
                .arg(static_cast<unsigned char>(data[static_cast<int>(i + j)]), 2, 16, QChar('0'))
                    .toUpper();
            } else {
                line += "   ";
            }
            if (j == 7) line += " ";
        }

        line += " ";
        for (qsizetype j = 0; j < 16 && i + j < dataSize; ++j) {
            unsigned char c = data[static_cast<int>(i + j)];
            line += (c >= 32 && c <= 126) ? QChar::fromLatin1(static_cast<char>(c)) : QChar('.');
        }
        result += line + "\n";
    }
    return result;
}
