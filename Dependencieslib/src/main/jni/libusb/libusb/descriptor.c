/**
 * modified to improve compatibility with some cameras.
 * Copyright(c) 2014 saki saki@serenegiant.com
 */
/* -*- Mode: C; indent-tabs-mode:t ; c-basic-offset:8 -*- */
/*
 * USB descriptor handling functions for libusb
 * Copyright © 2007 Daniel Drake <dsd@gentoo.org>
 * Copyright © 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#define LOG_TAG "libusb/descriptor"
#if 0    // デバッグ情報を出さない時1
#ifndef LOG_NDEBUG
#define	LOG_NDEBUG		// LOGV/LOGD/MARKを出力しない時
#endif
#undef USE_LOGALL			// 指定したLOGxだけを出力
#else
#define USE_LOGALL
#undef LOG_NDEBUG
#undef NDEBUG
#define GET_RAW_DESCRIPTOR
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libusbi.h"

/** @defgroup desc USB descriptors
 * This page details how to examine the various standard USB descriptors
 * for detected devices
 */

#define READ_LE16(p) ((uint16_t)    \
    (((uint16_t)((p)[1]) << 8) |    \
     ((uint16_t)((p)[0]))))

#define READ_LE32(p) ((uint32_t)    \
    (((uint32_t)((p)[3]) << 24) |    \
     ((uint32_t)((p)[2]) << 16) |    \
     ((uint32_t)((p)[1]) <<  8) |    \
     ((uint32_t)((p)[0]))))

static inline int is_known_descriptor_type(int type) {
    return ((type == LIBUSB_DT_ENDPOINT)
            || (type == LIBUSB_DT_INTERFACE)
            || (type == LIBUSB_DT_CONFIG)
            || (type == LIBUSB_DT_DEVICE));
}

/* set host_endian if the w values are already in host endian format,
 * as opposed to bus endian. */
static void usbi_parse_descriptor(const unsigned char *source, const char *descriptor,
                                  void *dest) {
    const unsigned char *sp = source;
    unsigned char *dp = dest;
    char field_type;

    while (*descriptor) {
        field_type = *descriptor++;
        switch (field_type) {
            case 'b':    /* 8-bit byte */
                *dp++ = *sp++;
                break;
            case 'w':    /* 16-bit word, convert from little endian to CPU */
                dp += ((uintptr_t) dp & 1);    /* Align to word boundary */

                *((uint16_t * )
                dp) = READ_LE16(sp);
                sp += 2;
                dp += 2;
                break;
            case 'd':    /* 32-bit word, convert from little endian to CPU */
                dp += 4 - ((uintptr_t) dp & 3);    /* Align to word boundary */

                *((uint32_t *) dp) = READ_LE32(sp);
                sp += 4;
                dp += 4;
                break;
            case 'u':    /* 16 byte UUID */
                memcpy(dp, sp, 16);
                sp += 16;
                dp += 16;
                break;
        }
    }
}

static void clear_endpoint(struct libusb_endpoint_descriptor *endpoint) {
    free((void *) endpoint->extra);
}

static int parse_endpoint(struct libusb_context *ctx,
                          struct libusb_endpoint_descriptor *endpoint, unsigned char *buffer,
                          int size) {
    ENTER();

    struct usbi_descriptor_header *header;
    unsigned char *extra;
    unsigned char *begin;
    int parsed = 0;
    int len;

    if UNLIKELY(size < LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/)
    {
        usbi_err(ctx, "short endpoint descriptor read %d/%d",
                 size, LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/);
        RETURN(LIBUSB_ERROR_IO,
        int);
    }

    header = (struct usbi_descriptor_header *) buffer;
    if UNLIKELY(header->bDescriptorType != LIBUSB_DT_ENDPOINT)
    {
        usbi_err(ctx, "unexpected descriptor %x (expected %x)",
                 header->bDescriptorType, LIBUSB_DT_ENDPOINT);
        RETURN(parsed,
        int);
    } else if (header->bLength < LIBUSB_DT_ENDPOINT_SIZE) {
        usbi_err(ctx, "invalid endpoint bLength (%u)", header->bLength);
        return LIBUSB_ERROR_IO;
    } else if UNLIKELY(header->bLength > size)
    {
        usbi_warn(ctx, "short endpoint descriptor read %d/%d",
                  size, header->bLength);
        RETURN(parsed,
        int);
    }
    if (header->bLength >= LIBUSB_DT_ENDPOINT_AUDIO_SIZE/*ENDPOINT_AUDIO_DESC_LENGTH*/)
        usbi_parse_descriptor(buffer, "bbbbwbbb", endpoint);
    else
        usbi_parse_descriptor(buffer, "bbbbwb", endpoint);

    buffer += header->bLength;
    size -= header->bLength;
    parsed += header->bLength;

    /* Skip over the rest of the Class Specific or Vendor Specific */
    /*  descriptors */
    begin = buffer;
    while (size >= LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
        header = (struct usbi_descriptor_header *) buffer;
        if UNLIKELY(header->bLength < LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/)
        {
            usbi_err(ctx, "invalid extra ep desc len (%d)",
                     header->bLength);
            RETURN(LIBUSB_ERROR_IO,
            int);
        } else if (header->bLength > size) {
            usbi_warn(ctx, "short extra ep desc read %d/%d",
                      size, header->bLength);
            RETURN(parsed,
            int);
        }

        /* If we find another "proper" descriptor then we're done  */
        if (is_known_descriptor_type(header->bDescriptorType))
            break;

        usbi_dbg("skipping descriptor 0x%02x", header->bDescriptorType);
        buffer += header->bLength;
        size -= header->bLength;
        parsed += header->bLength;
    }

    /* Copy any unknown descriptors into a storage area for drivers */
    /*  to later parse */
    len = (int) (buffer - begin);
    if (0 >= len) {
        RETURN(parsed,
        int);
    }

    extra = malloc(len);
    if UNLIKELY(!extra)
    {
        RETURN(LIBUSB_ERROR_NO_MEM,
        int);
    }

    memcpy(extra, begin, len);
    endpoint->extra = extra;
    endpoint->extra_length = len;

    RETURN(parsed,
    int);
}

static void clear_interface(struct libusb_interface *usb_interface) {
    int i;

    if (NULL != usb_interface->altsetting) {
        for (i = 0; i < usb_interface->num_altsetting; i++) {
            struct libusb_interface_descriptor *ifp =
                    (struct libusb_interface_descriptor *)
                            usb_interface->altsetting + i;
            if (NULL != ifp->extra)
                free((void *) ifp->extra);
            if (NULL != ifp->endpoint) {
                int j;
                for (j = 0; j < ifp->bNumEndpoints; j++)
                    clear_endpoint((struct libusb_endpoint_descriptor *)
                                           ifp->endpoint + j);
                free((void *) ifp->endpoint);
            }
        }
        free((void *) usb_interface->altsetting);
        usb_interface->altsetting = NULL;
    }
}

static int parse_interface(libusb_context *ctx,
                           struct libusb_interface *usb_interface, unsigned char *buffer,
                           int size) {
    ENTER();

    int len;
    int r;
    int parsed = 0;
    int interface_number = -1;
    struct usbi_descriptor_header *header;
    struct usbi_interface_descriptor *if_desc;
    struct libusb_interface_descriptor *ifp;
    unsigned char *begin;

    while (size >= LIBUSB_DT_INTERFACE_SIZE/*INTERFACE_DESC_LENGTH*/) {
        struct libusb_interface_descriptor *altsetting;
        altsetting = usbi_reallocf((void *) usb_interface->altsetting,
                                   sizeof(struct libusb_interface_descriptor) *
                                   (usb_interface->num_altsetting + 1));
        if UNLIKELY(!altsetting)
        {
            r = LIBUSB_ERROR_NO_MEM;
            goto err;
        }
        usb_interface->altsetting = altsetting;

        ifp = altsetting + usb_interface->num_altsetting;
        usbi_parse_descriptor(buffer, "bbbbbbbbb", ifp);
        if UNLIKELY(ifp->bDescriptorType != LIBUSB_DT_INTERFACE)
        {
            LOGE("unexpected descriptor %x (expected %x)",
                 ifp->bDescriptorType, LIBUSB_DT_INTERFACE);
            RETURN(parsed,
            int);
        } else if UNLIKELY(ifp->bLength < LIBUSB_DT_INTERFACE_SIZE/*INTERFACE_DESC_LENGTH*/)
        {
            LOGE("invalid interface bLength (%d)",
                 ifp->bLength);
            r = LIBUSB_ERROR_IO;
            goto err;
        } else if UNLIKELY(ifp->bLength > size)
        {
            LOGV("short intf descriptor read %d/%d",
                 size, ifp->bLength);
            RETURN(parsed,
            int);
        } else if UNLIKELY(ifp->bNumEndpoints > USB_MAXENDPOINTS)
        {
            LOGE("too many endpoints (%d)", ifp->bNumEndpoints);
            r = LIBUSB_ERROR_IO;
            goto err;
        }

        usb_interface->num_altsetting++;
        ifp->extra = NULL;
        ifp->extra_length = 0;
        ifp->endpoint = NULL;

        if (interface_number == -1)
            interface_number = ifp->bInterfaceNumber;

        /* Skip over the interface */
        buffer += ifp->bLength;
        parsed += ifp->bLength;
        size -= ifp->bLength;

        begin = buffer;

        /* Skip over any interface, class or vendor descriptors */
        while (size >= LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
            header = (struct usbi_descriptor_header *) buffer;
            if UNLIKELY(header->bLength < LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/)
            {
                usbi_err(ctx,
                         "invalid extra intf desc len (%d)",
                         header->bLength);
                r = LIBUSB_ERROR_IO;
                goto err;
            } else if (header->bLength > size) {
                usbi_warn(ctx,
                          "short extra intf desc read %d/%d",
                          size, header->bLength);
                RETURN(parsed,
                int);
            }

            //MARK("bDescriptorType=0x%02x", header->bDescriptorType);
            /* If we find another "proper" descriptor then we're done */
            if (is_known_descriptor_type(header->bDescriptorType))
                break;

            buffer += header->bLength;
            parsed += header->bLength;
            size -= header->bLength;
        }

        /* Copy any unknown descriptors into a storage area for */
        /*  drivers to later parse */
        len = (int) (buffer - begin);
        if (len > 0) {
            MARK("save unknown descriptors into ifp->extra:lebgth=%d", len);
            void *extra = malloc((size_t) len);
            if (!extra) {
                r = LIBUSB_ERROR_NO_MEM;
                goto err;
            }

            memcpy(extra, begin, len);
            ifp->extra = extra;
            ifp->extra_length = len;
        }

        //MARK("bNumEndpoints=%d", ifp->bNumEndpoints);
        if (ifp->bNumEndpoints > 0) {
            struct libusb_endpoint_descriptor *endpoint;
            int i;

            endpoint = calloc(ifp->bNumEndpoints, sizeof(*endpoint));
            if UNLIKELY(!endpoint)
            {
                r = LIBUSB_ERROR_NO_MEM;
                goto err;
            }

            ifp->endpoint = endpoint;
            for (i = 0; i < ifp->bNumEndpoints; i++) {
                //MARK("parse endpoint%d", i);
                r = parse_endpoint(ctx, endpoint + i, buffer, size);
                if UNLIKELY(r < 0)
                goto err;
                if (r == 0) {
                    ifp->bNumEndpoints = (uint8_t) i;
                    break;
                }

                buffer += r;
                parsed += r;
                size -= r;
            }
        }

        /* We check to see if it's an alternate to this one */
        if_desc = (struct usbi_interface_descriptor *) buffer;
        if (size < LIBUSB_DT_INTERFACE_SIZE ||
            if_desc->bDescriptorType != LIBUSB_DT_INTERFACE ||
            if_desc->bInterfaceNumber != interface_number)
            RETURN(parsed,
        int);
    }

    RETURN(parsed,
    int);
    err:
    clear_interface(usb_interface);
    RETURN(r,
    int);
}

#if 0
static void clear_association(struct libusb_association_descriptor *association) {
    if LIKELY(association && association->extra) {
        free((unsigned char *) association->extra);
        association->extra = NULL;
        association->extra_length = 0;
    }
}

static int parse_association(struct libusb_context *ctx,
        struct libusb_config_descriptor *config, unsigned char *buffer,
    int size) {

    ENTER();

    struct usbi_descriptor_header header;
    struct libusb_association_descriptor *association, *temp;
    unsigned char *begin;
    int parsed = 0;
    int len;

    if UNLIKELY(size < LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
        usbi_err(ctx, "short association descriptor read %d/%d",
             size, LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/);
        RETURN(LIBUSB_ERROR_IO, int);
    }
    // ディスクリプタの先頭2バイトだけ解析して長さとディスクリプタの種類を取得
    usbi_parse_descriptor(buffer, "bb", &header);
    if UNLIKELY(header.bDescriptorType != LIBUSB_DT_ASSOCIATION) {	// 種類が違う時
        usbi_err(ctx, "unexpected descriptor %x (expected %x)",
            header.bDescriptorType, LIBUSB_DT_ASSOCIATION);
        RETURN(parsed, int);	// return 0;
    }
    if UNLIKELY(header.bLength > size) {	// IADに長さが足りない時
        usbi_warn(ctx, "short association descriptor read %d/%d",
              size, header.bLength);
        RETURN(parsed, int);	// return 0;
    }
    if (header.bLength >= LIBUSB_DT_ASSOCIATION_SIZE/*ASSOCIATION_DESC_LENGTH*/) {
        config->association_descriptor = usbi_reallocf(config->association_descriptor,
            sizeof(struct libusb_association_descriptor) * (config->num_associations + 1));
        if UNLIKELY(!config->association_descriptor) {
            parsed = LIBUSB_ERROR_NO_MEM;
            goto err;
        }
        association = config->association_descriptor + config->num_associations;
        association->extra = NULL;
        association->extra_length = 0;
        len = usbi_parse_descriptor(buffer, "bbbbbbbb", association);
        if LIKELY(len > 0) {
            config->num_associations++;
#if 0
            LOGI("\t association:bLength=%d", association->bLength);
            LOGI("\t association:bDescriptorType=0x%02d", association->bDescriptorType);
            LOGI("\t association:bFirstInterface=%d", association->bFirstInterface);
            LOGI("\t association:bInterfaceCount=%d", association->bInterfaceCount);
            LOGI("\t association:bFunctionClass=0x%02x", association->bFunctionClass);
            LOGI("\t association:bFunctionSubClass=0x%02x", association->bFunctionSubClass);
            LOGI("\t association:bFunctionProtocol=0x%02x", association->bFunctionProtocol);
            LOGI("\t association:iFunction=%d", association->iFunction);
#endif
        } else {
            // 解析に失敗した時は未使用部分を削除
            config->association_descriptor = usbi_reallocf(association,
                sizeof(struct libusb_association_descriptor) * config->num_associations);
        }
    } else {
        // 種類はIADで有るにも関わらず長さが足りない時
        usbi_err(ctx, "invalid interface association descriptor bLength (%d)", header.bLength);
        RETURN(LIBUSB_ERROR_IO, int);
    }
    // 次の解析開始位置・残りサイズをセット
    buffer += header.bLength;
    size -= header.bLength;
    parsed += header.bLength;

    /* Skip over the rest of the Class Specific or Vendor Specific descriptors */
    begin = buffer;
    while (size >= LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
        usbi_parse_descriptor(buffer, "bb", &header);
        if UNLIKELY(header.bLength < LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
            usbi_err(ctx, "invalid extra ia desc len (%d)",
                 header.bLength);
            RETURN(LIBUSB_ERROR_IO, int);
        } else if (header.bLength > size) {
            usbi_warn(ctx, "short extra ia desc read %d/%d",
                  size, header.bLength);
            RETURN(parsed, int);
        }

        MARK("bDescriptorType=0x%02x", header.bDescriptorType);
        /* If we find another "proper" descriptor then we're done  */
        if (is_known_descriptor_type(header.bDescriptorType))
            break;

        usbi_dbg("skipping descriptor 0x%02x", header.bDescriptorType);
        buffer += header.bLength;
        size -= header.bLength;
        parsed += header.bLength;
    }

    // Append/Copy any unknown descriptors into a storage area for drivers to later parse
    len = (int)(buffer - begin);
    if (!len) {
        RETURN(parsed, int);
    }

    MARK("save unknown descriptors into config->extra:length=%d", len);
    config->extra = usbi_reallocf((unsigned char *)config->extra, config->extra_length + len);
    if UNLIKELY(!config->extra) {
        config->extra_length = 0;
        RETURN(LIBUSB_ERROR_NO_MEM, int);
    }
    memcpy((unsigned char *)config->extra + config->extra_length, begin, len);
    config->extra_length += len;

    RETURN(parsed, int);
err:
    clear_association(config->association_descriptor);
    config->association_descriptor = NULL;
    RETURN(parsed, int);
}
#endif

static void clear_configuration(struct libusb_config_descriptor *config) {
    if UNLIKELY(!config)
    return;

    if LIKELY(config->interface)
    {
        int i;
        for (i = 0; i < config->bNumInterfaces; i++)
            clear_interface((struct libusb_interface *)
                                    config->interface + i);
        free((void *) config->interface);
        config->interface = NULL;    // XXX
    }
    if (config->extra) {
        free((void *) config->extra);
        config->extra = NULL;    // XXX
    }
}

static int parse_configuration(struct libusb_context *ctx,
                               struct libusb_config_descriptor *config, unsigned char *buffer,
                               int size) {

    ENTER();

    int parsed_if;
    int r;
    struct usbi_descriptor_header *header;
    struct libusb_interface *usb_interface;

    if UNLIKELY(size < LIBUSB_DT_CONFIG_SIZE)
    {
        usbi_err(ctx, "short config descriptor read %d/%d",
                 size, LIBUSB_DT_CONFIG_SIZE);
        RETURN(LIBUSB_ERROR_IO,
        int);
    }

    usbi_parse_descriptor(buffer, "bbwbbbbb", config);
    if UNLIKELY(config->bDescriptorType != LIBUSB_DT_CONFIG)
    {
        usbi_err(ctx, "unexpected descriptor %x (expected %x)",
                 config->bDescriptorType, LIBUSB_DT_CONFIG);
        RETURN(LIBUSB_ERROR_IO,
        int);
    } else if UNLIKELY(config->bLength < LIBUSB_DT_CONFIG_SIZE)
    {
        usbi_err(ctx, "invalid config bLength (%d)", config->bLength);
        RETURN(LIBUSB_ERROR_IO,
        int);
    } else if UNLIKELY(config->bLength > size)
    {
        usbi_err(ctx, "short config descriptor read %d/%d",
                 size, config->bLength);
        RETURN(LIBUSB_ERROR_IO,
        int);
    } else if UNLIKELY(config->bNumInterfaces > USB_MAXINTERFACES)
    {
        usbi_err(ctx, "too many interfaces (%d)", config->bNumInterfaces);
        RETURN(LIBUSB_ERROR_IO,
        int);
    }
    // インターフェースディスクリプタ配列を確保(長さはconfig->bNumInterfaces)
    usb_interface = calloc(config->bNumInterfaces, sizeof(*usb_interface));
    // インターフェースディスクリプタ配列を確保できなかった
    if UNLIKELY(!usb_interface)
    RETURN(LIBUSB_ERROR_NO_MEM,
    int);

    config->interface = usb_interface;

    buffer += config->bLength;
    size -= config->bLength;

    MARK("bNumInterfaces=%d", config->bNumInterfaces);
    for (parsed_if = 0; parsed_if < config->bNumInterfaces; parsed_if++) {
        int len;
        unsigned char *begin;

        /* Skip over the rest of the Class Specific or Vendor Specific descriptors */
        begin = buffer;
        while (size >= LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
            header = (struct usbi_descriptor_header *) buffer;
            if UNLIKELY(header->bLength < LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/)
            {
                usbi_err(ctx,
                         "invalid extra config desc len (%d)",
                         header->bLength);
                r = LIBUSB_ERROR_IO;
                goto err;
            } else if UNLIKELY(header->bLength > size)
            {
                usbi_warn(ctx,
                          "short extra config desc read %d/%d",
                          size, header->bLength);
                config->bNumInterfaces = (uint8_t) parsed_if;
                return size;
            }

            MARK("bDescriptorType=0x%02x", header->bDescriptorType);
            /* If we find another "proper" descriptor then we're done */
            if (is_known_descriptor_type(header->bDescriptorType))
                break;

            usbi_dbg("skipping descriptor 0x%02x\n", header->bDescriptorType);
            buffer += header->bLength;
            size -= header->bLength;
        }

        /* Copy any unknown descriptors into a storage area for */
        /*  drivers to later parse */
        len = (int) (buffer - begin);
        if (len > 0) {
            MARK("save skipped unknown descriptors into config->extra:len=%d", len);
            uint8_t *extra = realloc((void *) config->extra,
                                     (size_t)(config->extra_length + len));

            if (!extra) {
                r = LIBUSB_ERROR_NO_MEM;
                goto err;
            }

            memcpy(extra + config->extra_length, begin, len);
            config->extra = extra;
            config->extra_length += len;
        }

        r = parse_interface(ctx, usb_interface + parsed_if, buffer, size);
        if (r < 0)
            goto err;
        if (r == 0) {
            config->bNumInterfaces = parsed_if;
            break;
        }

        buffer += r;
        size -= r;
    }
    RETURN(size,
    int);

    err:
    clear_configuration(config);
    RETURN(r,
    int);
}

#if PRINT_DIAG
static void dump_descriptors(unsigned char *buffer, int size) {
    struct usbi_descriptor_header header;
    struct libusb_config_descriptor config;
    struct libusb_interface_descriptor interface;
    struct libusb_endpoint_descriptor endpoint;
    int i;

    LOGI("DUMP DESCRIPTIONS");
    for (i = 0; size >= 0; i += header.bLength, size -= header.bLength) {
        if (size == 0) {
            LOGI("END");
            return;
        }

        if (size < LIBUSB_DT_HEADER_SIZE) {
            LOGE("short descriptor read %d/2", size);
            return;
        }
        usbi_parse_descriptor(buffer + i, "bb", &header);
        switch (header.bDescriptorType) {
        case LIBUSB_DT_DEVICE:
            LOGI("LIBUSB_DT_DEVICE(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_CONFIG:
            usbi_parse_descriptor(buffer, "bbwbbbbb", &config);
            LOGI("LIBUSB_DT_CONFIG(0x%02x)", config.bDescriptorType);
            LOGI("\tbLength=%d", config.bLength);
            LOGI("\tbDescriptorType=0x%02x", config.bDescriptorType);
            LOGI("\twTotalLength=%d", config.wTotalLength);
            LOGI("\tbNumInterfaces=%d", config.bNumInterfaces);
            LOGI("\tbConfigurationValue=%d", config.bConfigurationValue);
            LOGI("\tiConfiguration=%d", config.iConfiguration);
            LOGI("\tbmAttributes=%d", config.bmAttributes);
            LOGI("\tMaxPower=%d", config.MaxPower);
            LOGI("\textra_length=%d", config.bLength - LIBUSB_DT_CONFIG_SIZE);
            break;
        case LIBUSB_DT_STRING:
            LOGI("LIBUSB_DT_STRING(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_INTERFACE:
            usbi_parse_descriptor(buffer + i, "bbbbbbbbb", &interface);
            LOGI("LIBUSB_DT_INTERFACE(0x%02x):", header.bDescriptorType);
            LOGI("\tbLength=%d", interface.bLength);
            LOGI("\tbDescriptorType=0x%02x", interface.bDescriptorType);
            LOGI("\tbInterfaceNumber=%d", interface.bInterfaceNumber);
            LOGI("\tbAlternateSetting=%d", interface.bAlternateSetting);
            LOGI("\tbNumEndpoints=%d", interface.bNumEndpoints);
            LOGI("\tbInterfaceClass=0x%02x", interface.bInterfaceClass);
            LOGI("\tbInterfaceSubClass=0x%02x", interface.bInterfaceSubClass);
            LOGI("\tbInterfaceProtocol=0x%02x", interface.bInterfaceProtocol);
            LOGI("\tiInterface=%d", interface.iInterface);
            LOGI("\textra_length=%d", interface.bLength - LIBUSB_DT_INTERFACE_SIZE);
            break;
        case LIBUSB_DT_ENDPOINT:
            usbi_parse_descriptor(buffer + i, "bbbbwbbb", &endpoint);
            LOGI("LIBUSB_DT_ENDPOINT(0x%02x):", header.bDescriptorType);
            LOGI("\tbLength=%d", endpoint.bLength);
            LOGI("\tbDescriptorType=0x%02x", endpoint.bDescriptorType);
            LOGI("\tbEndpointAddress=%d", endpoint.bEndpointAddress);
            LOGI("\tbmAttributes=%d", endpoint.bmAttributes);
            LOGI("\twMaxPacketSize=%d", endpoint.wMaxPacketSize);
            LOGI("\tbInterval=%d", endpoint.bInterval);
            LOGI("\tbRefresh=%d", endpoint.bRefresh);
            LOGI("\tbSynchAddress=%d", endpoint.bSynchAddress);
            LOGI("\textra_length=%d", endpoint.bLength - LIBUSB_DT_ENDPOINT_SIZE);
            break;
        case LIBUSB_DT_DEVICE_QUALIFIER:
            LOGI("LIBUSB_DT_DEVICE_QUALIFIER(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            LOGI("\textra_length=%d", header.bLength - LIBUSB_DT_QUALIFER_SIZE);
            break;
        case LIBUSB_DT_OTHER_SPEED_CONFIGURATION:
            LOGI("LIBUSB_DT_OTHER_SPEED_CONFIGURATION(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            LOGI("\textra_length=%d", header.bLength - LIBUSB_DT_OTHER_SPEED_SIZE);
            break;
        case LIBUSB_DT_INTERFACE_POWER:
            LOGI("LIBUSB_DT_INTERFACE_POWER(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_OTG:
            LOGI("LIBUSB_DT_OTG(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_DEBUG:
            LOGI("LIBUSB_DT_DEBUG(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_ASSOCIATION:
            LOGI("LIBUSB_DT_ASSOCIATION(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            LOGI("\textra_length=%d", header.bLength - LIBUSB_DT_ASSOCIATION_SIZE);
            break;
        case LIBUSB_DT_BOS:
            LOGI("LIBUSB_DT_BOS(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            LOGI("\textra_length=%d", header.bLength - LIBUSB_DT_BOS_SIZE);
            break;
        case LIBUSB_DT_DEVICE_CAPABILITY:
            LOGI("LIBUSB_DT_DEVICE_CAPABILITY(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            LOGI("\textra_length=%d", header.bLength - LIBUSB_DT_DEVICE_CAPABILITY_SIZE);
            break;
        case LIBUSB_DT_HID:
            LOGI("LIBUSB_DT_HID(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_HID_REPORT:
            LOGI("LIBUSB_DT_REPORT(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_HID_PHYSICAL:
            LOGI("LIBUSB_DT_PHYSICAL(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_CS_INTERFACE:
            LOGI("LIBUSB_DT_CS_INTERFACE(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_CS_ENDPOINT:
            LOGI("LIBUSB_DT_CS_ENDPOINT(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_HUB:
            LOGI("LIBUSB_DT_HUB(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_SUPERSPEED_HUB:
            LOGI("LIBUSB_DT_SUPERSPEED_HUB(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        case LIBUSB_DT_SS_ENDPOINT_COMPANION:
            LOGI("LIBUSB_DT_SS_ENDPOINT_COMPANION(0x%02x),length=%d", header.bDescriptorType, header.bLength);
            break;
        default:
            LOGI("unknown Descriptor(0x%02x),length=0x%02x", header.bDescriptorType, header.bLength);
            break;
        }
    }
}
#endif

static int raw_desc_to_config(struct libusb_context *ctx,
                              unsigned char *buf, int size,
                              struct libusb_config_descriptor **config) {
    ENTER();

    struct libusb_config_descriptor *_config = calloc(1, sizeof(*_config));
    int r;

    if UNLIKELY(!_config)
    RETURN(LIBUSB_ERROR_NO_MEM,
    int);

#if PRINT_DIAG
    dump_descriptors(buf, size);
#endif
    r = parse_configuration(ctx, _config, buf, size);
    if UNLIKELY(r < 0)
    {
        MARK("parse_configuration failed with error %d", r);
        free(_config);
        return r;
    } else if (r > 0) {
        MARK("still %d bytes of descriptor data left", r);
    }

    *config = _config;
    RETURN(LIBUSB_SUCCESS,
    int);
}

static int get_active_config_descriptor(struct libusb_device *dev,
                                        uint8_t *buffer, size_t size) {
    int r = usbi_backend->get_active_config_descriptor(dev, buffer, size);

    if (r < 0)
        return r;

    if (r < LIBUSB_DT_CONFIG_SIZE) {
        usbi_err(DEVICE_CTX(dev), "short config descriptor read %d/%d",
                 r, LIBUSB_DT_CONFIG_SIZE);
        LOGD("short config descriptor read %d/%d", r, LIBUSB_DT_CONFIG_SIZE);
        return LIBUSB_ERROR_IO;
    } else if (r != (int) size) {
        usbi_warn(DEVICE_CTX(dev), "short config descriptor read %d/%d",
                  r, (int) size);
        LOGD("short config descriptor read %d/%d", r, (int) size);
    }

    return r;
}

static int get_config_descriptor(struct libusb_device *dev, uint8_t config_idx,
                                 uint8_t *buffer, size_t size) {
    int r = usbi_backend->get_config_descriptor(dev, config_idx, buffer, size);

    if (r < 0)
        return r;
    if (r < LIBUSB_DT_CONFIG_SIZE) {
        usbi_err(DEVICE_CTX(dev), "short config descriptor read %d/%d",
                 r, LIBUSB_DT_CONFIG_SIZE);
        return LIBUSB_ERROR_IO;
    } else if (r != (int) size) {
        usbi_warn(DEVICE_CTX(dev), "short config descriptor read %d/%d",
                  r, (int) size);
    }

    return r;
}

/** \ingroup desc
 * Get the USB device descriptor for a given device.
 *
 * This is a non-blocking function; the device descriptor is cached in memory.
 *
 * Note since libusb-1.0.16, \ref LIBUSB_API_VERSION >= 0x01000102, this
 * function always succeeds.
 *
 * \param dev the device
 * \param desc output location for the descriptor data
 * \returns 0 on success or a LIBUSB_ERROR code on failure
 */
int API_EXPORTED libusb_get_device_descriptor(libusb_device *dev,
                                              struct libusb_device_descriptor *desc) {
    // FIXME add IAD support
    LOGD("desc=%p,dev=%p,device_descriptor=%p", desc, dev, &dev->device_descriptor);

    static_assert(sizeof(dev->device_descriptor) == LIBUSB_DT_DEVICE_SIZE,
                  "struct libusb_device_descriptor is not expected size");
    *desc = dev->device_descriptor;
    return 0;
}

/** \ingroup desc
 * Get the USB configuration descriptor for the currently active configuration.
 * This is a non-blocking function which does not involve any requests being
 * sent to the device.
 *
 * \param dev a device
 * \param config output location for the USB configuration descriptor. Only
 * valid if 0 was returned. Must be freed with libusb_free_config_descriptor()
 * after use.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the device is in unconfigured state
 * \returns another LIBUSB_ERROR code on error
 * \see libusb_get_config_descriptor
 */
int API_EXPORTED libusb_get_active_config_descriptor(libusb_device *dev,
                                                     struct libusb_config_descriptor **config) {
    union usbi_config_desc_buf _config;
    uint16_t config_len;
    unsigned char *buf = NULL;
    int r;

    r = get_active_config_descriptor(dev, _config.buf, sizeof(_config.buf));
    if (r < 0)
        return r;

    config_len = libusb_le16_to_cpu(_config.desc.wTotalLength);
    buf = malloc(config_len);
    if UNLIKELY(!buf)
    return LIBUSB_ERROR_NO_MEM;

    r = get_active_config_descriptor(dev, buf, config_len);
    if (r >= 0)
        r = raw_desc_to_config(DEVICE_CTX(dev), buf, r, config);

    free(buf);
    return r;
}

/** \ingroup desc
 * Get a USB configuration descriptor based on its index.
 * This is a non-blocking function which does not involve any requests being
 * sent to the device.
 *
 * \param dev a device
 * \param config_index the index of the configuration you wish to retrieve
 * \param config output location for the USB configuration descriptor. Only
 * valid if 0 was returned. Must be freed with libusb_free_config_descriptor()
 * after use.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
 * \returns another LIBUSB_ERROR code on error
 * \see libusb_get_active_config_descriptor()
 * \see libusb_get_config_descriptor_by_value()
 */
int API_EXPORTED libusb_get_config_descriptor(libusb_device *dev,
                                              uint8_t config_index,
                                              struct libusb_config_descriptor **config) {
    union usbi_config_desc_buf _config;
    uint16_t config_len;
    unsigned char *buf = NULL;
    int r;

    MARK("index %u", config_index);
    if (config_index >= dev->device_descriptor.bNumConfigurations)
        return LIBUSB_ERROR_NOT_FOUND;

    r = get_config_descriptor(dev, config_index, _config.buf, sizeof(_config.buf));
    if (r < 0)
        return r;

    config_len = libusb_le16_to_cpu(_config.desc.wTotalLength);
    buf = malloc(config_len);
    if (!buf)
        return LIBUSB_ERROR_NO_MEM;

    r = get_config_descriptor(dev, config_index, buf, config_len);

    if (r >= 0)
        r = raw_desc_to_config(DEVICE_CTX(dev), buf, r, config);

    free(buf);
    return r;
}

/** \ingroup desc
 * Get a USB configuration descriptor with a specific bConfigurationValue.
 * This is a non-blocking function which does not involve any requests being
 * sent to the device.
 *
 * \param dev a device
 * \param bConfigurationValue the bConfigurationValue of the configuration you
 * wish to retrieve
 * \param config output location for the USB configuration descriptor. Only
 * valid if 0 was returned. Must be freed with libusb_free_config_descriptor()
 * after use.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
 * \returns another LIBUSB_ERROR code on error
 * \see libusb_get_active_config_descriptor()
 * \see libusb_get_config_descriptor()
 */
int API_EXPORTED libusb_get_config_descriptor_by_value(libusb_device *dev,
                                                       uint8_t bConfigurationValue,
                                                       struct libusb_config_descriptor **config) {
    int r, idx;
    unsigned char *buf = NULL;

    if (usbi_backend->get_config_descriptor_by_value) {
        r = usbi_backend->get_config_descriptor_by_value(dev,
                                                         bConfigurationValue, &buf);
        if UNLIKELY(r < 0)
        return r;
        return raw_desc_to_config(dev->ctx, buf, r, config);
    }

    usbi_dbg(DEVICE_CTX(dev), "value %u", bConfigurationValue);
    for (idx = 0; idx < dev->device_descriptor.bNumConfigurations; idx++) {
        union usbi_config_desc_buf _config;

        r = get_config_descriptor(dev, idx, _config.buf, sizeof(_config.buf));
        if (r < 0)
            return r;

        if (_config.desc.bConfigurationValue == bConfigurationValue)
            return libusb_get_config_descriptor(dev, idx, config);
    }

    return LIBUSB_ERROR_NOT_FOUND;
}

/** \ingroup desc
 * Free a configuration descriptor obtained from
 * libusb_get_active_config_descriptor() or libusb_get_config_descriptor().
 * It is safe to call this function with a NULL config parameter, in which
 * case the function simply returns.
 *
 * \param config the configuration descriptor to free
 */
void API_EXPORTED libusb_free_config_descriptor(
        struct libusb_config_descriptor *config) {
    if UNLIKELY(!config)
    return;

    clear_configuration(config);
    free(config);
}

/** \ingroup desc
 * Get an endpoints superspeed endpoint companion descriptor (if any)
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param endpoint endpoint descriptor from which to get the superspeed
 * endpoint companion descriptor
 * \param ep_comp output location for the superspeed endpoint companion
 * descriptor. Only valid if 0 was returned. Must be freed with
 * libusb_free_ss_endpoint_companion_descriptor() after use.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
 * \returns another LIBUSB_ERROR code on error
 */
int API_EXPORTED libusb_get_ss_endpoint_companion_descriptor(
        struct libusb_context *ctx,
        const struct libusb_endpoint_descriptor *endpoint,
        struct libusb_ss_endpoint_companion_descriptor **ep_comp) {
    struct usbi_descriptor_header *header;
    int size = endpoint->extra_length;
    const unsigned char *buffer = endpoint->extra;

    *ep_comp = NULL;

    while (size >= LIBUSB_DT_HEADER_SIZE/*DESC_HEADER_LENGTH*/) {
        header = (struct usbi_descriptor_header *) buffer;
        if (header->bDescriptorType != LIBUSB_DT_SS_ENDPOINT_COMPANION) {
            if UNLIKELY(header->bLength < LIBUSB_DT_HEADER_SIZE)
            {
                usbi_err(ctx, "invalid descriptor length %d",
                         header->bLength);
                return LIBUSB_ERROR_IO;
            }
            buffer += header->bLength;
            size -= header->bLength;
            continue;
        } else if (header->bLength < LIBUSB_DT_SS_ENDPOINT_COMPANION_SIZE) {
            usbi_err(ctx, "invalid ss-ep-comp-desc length %u",
                     header->bLength);
            return LIBUSB_ERROR_IO;
        } else if (header->bLength > size) {
            usbi_err(ctx, "short ss-ep-comp-desc read %d/%u",
                     size, header->bLength);
            return LIBUSB_ERROR_IO;
        }

        *ep_comp = malloc(sizeof(**ep_comp));
        if (!*ep_comp)
            return LIBUSB_ERROR_NO_MEM;
        usbi_parse_descriptor(buffer, "bbbbw", *ep_comp);
        return LIBUSB_SUCCESS;
    }
    return LIBUSB_ERROR_NOT_FOUND;
}

/** \ingroup desc
 * Free a superspeed endpoint companion descriptor obtained from
 * libusb_get_ss_endpoint_companion_descriptor().
 * It is safe to call this function with a NULL ep_comp parameter, in which
 * case the function simply returns.
 *
 * \param ep_comp the superspeed endpoint companion descriptor to free
 */
void API_EXPORTED libusb_free_ss_endpoint_companion_descriptor(
        struct libusb_ss_endpoint_companion_descriptor *ep_comp) {
    free(ep_comp);
}

static int parse_bos(struct libusb_context *ctx,
                     struct libusb_bos_descriptor **bos,
                     unsigned char *buffer, int size) {
    struct libusb_bos_descriptor bos_header, *_bos;
    const struct usbi_bos_descriptor *bos_desc;
    const struct usbi_descriptor_header *header;
    int i;

    if UNLIKELY(size < LIBUSB_DT_BOS_SIZE)
    {
        usbi_err(ctx, "short bos descriptor read %d/%d",
                 size, LIBUSB_DT_BOS_SIZE);
        return LIBUSB_ERROR_IO;
    }

    bos_desc = (const struct usbi_bos_descriptor *) buffer;
    if UNLIKELY(bos_desc->bDescriptorType != LIBUSB_DT_BOS)
    {
        usbi_err(ctx, "unexpected descriptor %x (expected %x)",
                 bos_desc->bDescriptorType, LIBUSB_DT_BOS);
        return LIBUSB_ERROR_IO;
    }
    if UNLIKELY(bos_desc->bLength < LIBUSB_DT_BOS_SIZE)
    {
        usbi_err(ctx, "invalid bos bLength (%d)", bos_desc->bLength);
        return LIBUSB_ERROR_IO;
    }
    if UNLIKELY(bos_desc->bLength > size)
    {
        usbi_err(ctx, "short bos descriptor read %d/%d",
                 size, bos_desc->bLength);
        return LIBUSB_ERROR_IO;
    }

    _bos = calloc(1, sizeof(*_bos) + bos_desc->bNumDeviceCaps * sizeof(void *));
    if UNLIKELY(!_bos)
    return LIBUSB_ERROR_NO_MEM;

    usbi_parse_descriptor(buffer, "bbwb", _bos);
    buffer += _bos->bLength;
    size -= _bos->bLength;

    /* Get the device capability descriptors */
    for (i = 0; i < _bos->bNumDeviceCaps; i++) {
        if (size < LIBUSB_DT_DEVICE_CAPABILITY_SIZE) {
            usbi_warn(ctx, "short dev-cap descriptor read %d/%d",
                      size, LIBUSB_DT_DEVICE_CAPABILITY_SIZE);
            break;
        }
        header = (const struct usbi_descriptor_header *) buffer;
        if (header->bDescriptorType != LIBUSB_DT_DEVICE_CAPABILITY) {
            usbi_warn(ctx, "unexpected descriptor %x (expected %x)",
                      header->bDescriptorType, LIBUSB_DT_DEVICE_CAPABILITY);
            break;
        } else if UNLIKELY(header->bLength < LIBUSB_DT_DEVICE_CAPABILITY_SIZE)
        {
            usbi_err(ctx, "invalid dev-cap bLength (%d)",
                     header->bLength);
            libusb_free_bos_descriptor(_bos);
            return LIBUSB_ERROR_IO;
        } else if (header->bLength > size) {
            usbi_warn(ctx, "short dev-cap descriptor read %d/%d",
                      size, header->bLength);
            break;
        }

        _bos->dev_capability[i] = malloc(header->bLength);
        if UNLIKELY(!_bos->dev_capability[i])
        {
            libusb_free_bos_descriptor(_bos);
            return LIBUSB_ERROR_NO_MEM;
        }
        memcpy(_bos->dev_capability[i], buffer, header->bLength);
        buffer += header->bLength;
        size -= header->bLength;
    }
    _bos->bNumDeviceCaps = (uint8_t) i;
    *bos = _bos;

    return LIBUSB_SUCCESS;
}

/** \ingroup desc
 * Get a Binary Object Store (BOS) descriptor
 * This is a BLOCKING function, which will send requests to the device.
 *
 * \param handle the handle of an open libusb device
 * \param bos output location for the BOS descriptor. Only valid if 0 was returned.
 * Must be freed with \ref libusb_free_bos_descriptor() after use.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the device doesn't have a BOS descriptor
 * \returns another LIBUSB_ERROR code on error
 */
int API_EXPORTED libusb_get_bos_descriptor(libusb_device_handle *handle,
                                           struct libusb_bos_descriptor **bos) {
    union usbi_bos_desc_buf _bos;
    uint16_t bos_len;
    unsigned char *bos_data = NULL;
    int r;

    /* Read the BOS. This generates 2 requests on the bus,
     * one for the header, and one for the full BOS */
    r = libusb_get_descriptor(handle, LIBUSB_DT_BOS, 0, _bos.buf, sizeof(_bos.buf));
    if UNLIKELY(r < 0)
    {
        if (r != LIBUSB_ERROR_PIPE)
            usbi_err(handle->dev->ctx, "failed to read BOS (%d)", r);
        return r;
    }
    if UNLIKELY(r < LIBUSB_DT_BOS_SIZE)
    {
        usbi_err(handle->dev->ctx, "short BOS read %d/%d",
                 r, LIBUSB_DT_BOS_SIZE);
        return LIBUSB_ERROR_IO;
    }

    bos_len = libusb_le16_to_cpu(_bos.desc.wTotalLength);
    usbi_dbg("found BOS descriptor: size %d bytes, %d capabilities",
             bos_len, _bos.desc.bNumDeviceCaps);
    bos_data = calloc(1, bos_len);
    if UNLIKELY(!bos_data)
    return LIBUSB_ERROR_NO_MEM;

    r = libusb_get_descriptor(handle, LIBUSB_DT_BOS, 0, bos_data, bos_len);
    if LIKELY(r >= 0)
    r = parse_bos(handle->dev->ctx, bos, bos_data, r);
    else
    usbi_err(handle->dev->ctx, "failed to read BOS (%d)", r);

    free(bos_data);
    return r;
}

/** \ingroup desc
 * Free a BOS descriptor obtained from libusb_get_bos_descriptor().
 * It is safe to call this function with a NULL bos parameter, in which
 * case the function simply returns.
 *
 * \param bos the BOS descriptor to free
 */
void API_EXPORTED libusb_free_bos_descriptor(struct libusb_bos_descriptor *bos) {
    int i;

    if (!bos)
        return;

    for (i = 0; i < bos->bNumDeviceCaps; i++)
        free(bos->dev_capability[i]);
    free(bos);
}

/** \ingroup desc
 * Get an USB 2.0 Extension descriptor
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param dev_cap Device Capability descriptor with a bDevCapabilityType of
 * \ref libusb_capability_type::LIBUSB_BT_USB_2_0_EXTENSION
 * LIBUSB_BT_USB_2_0_EXTENSION
 * \param usb_2_0_extension output location for the USB 2.0 Extension
 * descriptor. Only valid if 0 was returned. Must be freed with
 * libusb_free_usb_2_0_extension_descriptor() after use.
 * \returns 0 on success
 * \returns a LIBUSB_ERROR code on error
 */
int API_EXPORTED libusb_get_usb_2_0_extension_descriptor(
        struct libusb_context *ctx,
        struct libusb_bos_dev_capability_descriptor *dev_cap,
        struct libusb_usb_2_0_extension_descriptor **usb_2_0_extension) {
    struct libusb_usb_2_0_extension_descriptor *_usb_2_0_extension;

    if UNLIKELY(dev_cap->bDevCapabilityType != LIBUSB_BT_USB_2_0_EXTENSION)
    {
        usbi_err(ctx, "unexpected bDevCapabilityType %x (expected %x)",
                 dev_cap->bDevCapabilityType,
                 LIBUSB_BT_USB_2_0_EXTENSION);
        return LIBUSB_ERROR_INVALID_PARAM;
    } else if UNLIKELY(dev_cap->bLength < LIBUSB_BT_USB_2_0_EXTENSION_SIZE)
    {
        usbi_err(ctx, "short dev-cap descriptor read %d/%d",
                 dev_cap->bLength, LIBUSB_BT_USB_2_0_EXTENSION_SIZE);
        return LIBUSB_ERROR_IO;
    }

    _usb_2_0_extension = malloc(sizeof(*_usb_2_0_extension));
    if UNLIKELY(!_usb_2_0_extension)
    return LIBUSB_ERROR_NO_MEM;

    usbi_parse_descriptor((unsigned char *) dev_cap, "bbbd",
                          _usb_2_0_extension);

    *usb_2_0_extension = _usb_2_0_extension;
    return LIBUSB_SUCCESS;
}

/** \ingroup desc
 * Free a USB 2.0 Extension descriptor obtained from
 * libusb_get_usb_2_0_extension_descriptor().
 * It is safe to call this function with a NULL usb_2_0_extension parameter,
 * in which case the function simply returns.
 *
 * \param usb_2_0_extension the USB 2.0 Extension descriptor to free
 */
void API_EXPORTED libusb_free_usb_2_0_extension_descriptor(
        struct libusb_usb_2_0_extension_descriptor *usb_2_0_extension) {
    free(usb_2_0_extension);
}

/** \ingroup desc
 * Get a SuperSpeed USB Device Capability descriptor
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param dev_cap Device Capability descriptor with a bDevCapabilityType of
 * \ref libusb_capability_type::LIBUSB_BT_SS_USB_DEVICE_CAPABILITY
 * LIBUSB_BT_SS_USB_DEVICE_CAPABILITY
 * \param ss_usb_device_cap output location for the SuperSpeed USB Device
 * Capability descriptor. Only valid if 0 was returned. Must be freed with
 * libusb_free_ss_usb_device_capability_descriptor() after use.
 * \returns 0 on success
 * \returns a LIBUSB_ERROR code on error
 */
int API_EXPORTED libusb_get_ss_usb_device_capability_descriptor(
        struct libusb_context *ctx,
        struct libusb_bos_dev_capability_descriptor *dev_cap,
        struct libusb_ss_usb_device_capability_descriptor **ss_usb_device_cap) {
    struct libusb_ss_usb_device_capability_descriptor *_ss_usb_device_cap;

    if UNLIKELY(dev_cap->bDevCapabilityType != LIBUSB_BT_SS_USB_DEVICE_CAPABILITY)
    {
        usbi_err(ctx, "unexpected bDevCapabilityType %x (expected %x)",
                 dev_cap->bDevCapabilityType,
                 LIBUSB_BT_SS_USB_DEVICE_CAPABILITY);
        return LIBUSB_ERROR_INVALID_PARAM;
    } else if UNLIKELY(dev_cap->bLength < LIBUSB_BT_SS_USB_DEVICE_CAPABILITY_SIZE)
    {
        usbi_err(ctx, "short dev-cap descriptor read %d/%d",
                 dev_cap->bLength, LIBUSB_BT_SS_USB_DEVICE_CAPABILITY_SIZE);
        return LIBUSB_ERROR_IO;
    }

    _ss_usb_device_cap = malloc(sizeof(*_ss_usb_device_cap));
    if UNLIKELY(!_ss_usb_device_cap)
    return LIBUSB_ERROR_NO_MEM;

    usbi_parse_descriptor((unsigned char *) dev_cap, "bbbbwbbw",
                          _ss_usb_device_cap);

    *ss_usb_device_cap = _ss_usb_device_cap;
    return LIBUSB_SUCCESS;
}

/** \ingroup desc
 * Free a SuperSpeed USB Device Capability descriptor obtained from
 * libusb_get_ss_usb_device_capability_descriptor().
 * It is safe to call this function with a NULL ss_usb_device_cap
 * parameter, in which case the function simply returns.
 *
 * \param ss_usb_device_cap the USB 2.0 Extension descriptor to free
 */
void API_EXPORTED libusb_free_ss_usb_device_capability_descriptor(
        struct libusb_ss_usb_device_capability_descriptor *ss_usb_device_cap) {
    free(ss_usb_device_cap);
}

/** \ingroup desc
 * Get a Container ID descriptor
 *
 * \param ctx the context to operate on, or NULL for the default context
 * \param dev_cap Device Capability descriptor with a bDevCapabilityType of
 * \ref libusb_capability_type::LIBUSB_BT_CONTAINER_ID
 * LIBUSB_BT_CONTAINER_ID
 * \param container_id output location for the Container ID descriptor.
 * Only valid if 0 was returned. Must be freed with
 * libusb_free_container_id_descriptor() after use.
 * \returns 0 on success
 * \returns a LIBUSB_ERROR code on error
 */
int API_EXPORTED libusb_get_container_id_descriptor(struct libusb_context *ctx,
                                                    struct libusb_bos_dev_capability_descriptor *dev_cap,
                                                    struct libusb_container_id_descriptor **container_id) {
    struct libusb_container_id_descriptor *_container_id;

    if UNLIKELY(dev_cap->bDevCapabilityType != LIBUSB_BT_CONTAINER_ID)
    {
        usbi_err(ctx, "unexpected bDevCapabilityType %x (expected %x)",
                 dev_cap->bDevCapabilityType,
                 LIBUSB_BT_CONTAINER_ID);
        return LIBUSB_ERROR_INVALID_PARAM;
    } else if UNLIKELY(dev_cap->bLength < LIBUSB_BT_CONTAINER_ID_SIZE)
    {
        usbi_err(ctx, "short dev-cap descriptor read %d/%d",
                 dev_cap->bLength, LIBUSB_BT_CONTAINER_ID_SIZE);
        return LIBUSB_ERROR_IO;
    }

    _container_id = malloc(sizeof(*_container_id));
    if UNLIKELY(!_container_id)
    return LIBUSB_ERROR_NO_MEM;

    usbi_parse_descriptor((unsigned char *) dev_cap, "bbbbu",
                          _container_id);

    *container_id = _container_id;
    return LIBUSB_SUCCESS;
}

/** \ingroup desc
 * Free a Container ID descriptor obtained from
 * libusb_get_container_id_descriptor().
 * It is safe to call this function with a NULL container_id parameter,
 * in which case the function simply returns.
 *
 * \param container_id the USB 2.0 Extension descriptor to free
 */
void API_EXPORTED libusb_free_container_id_descriptor(
        struct libusb_container_id_descriptor *container_id) {
    free(container_id);
}

/** \ingroup desc
 * Retrieve a string descriptor in C style ASCII.
 *
 * Wrapper around libusb_get_string_descriptor(). Uses the first language
 * supported by the device.
 *
 * \param dev a device handle
 * \param desc_index the index of the descriptor to retrieve
 * \param data output buffer for ASCII string descriptor
 * \param length size of data buffer
 * \returns number of bytes returned in data, or LIBUSB_ERROR code on failure
 */
int API_EXPORTED libusb_get_string_descriptor_ascii(libusb_device_handle *dev_handle,
                                                    uint8_t desc_index, unsigned char *data,
                                                    int length) {
    union usbi_string_desc_buf str;
    int r, si, di;
    uint16_t langid, wdata;

    /* Asking for the zero'th index is special - it returns a string
     * descriptor that contains all the language IDs supported by the
     * device. Typically there aren't many - often only one. Language
     * IDs are 16 bit numbers, and they start at the third byte in the
     * descriptor. There's also no point in trying to read descriptor 0
     * with this function. See USB 2.0 specification section 9.6.7 for
     * more information.
     */

    if UNLIKELY(!desc_index)
    return LIBUSB_ERROR_INVALID_PARAM;

    r = libusb_get_string_descriptor(dev_handle, 0, 0, str.buf, 4);
    if UNLIKELY(r < 0)
    return r;
    else if (r != 4 || str.desc.bLength < 4)
        return LIBUSB_ERROR_IO;
    else if (str.desc.bDescriptorType != LIBUSB_DT_STRING)
        return LIBUSB_ERROR_IO;
    else if (str.desc.bLength & 1)
        usbi_warn(HANDLE_CTX(dev_handle), "suspicious bLength %u for language ID string descriptor",
                  str.desc.bLength);

    langid = libusb_le16_to_cpu(str.desc.wData[0]);
    r = libusb_get_string_descriptor(dev_handle, desc_index, langid, str.buf, sizeof(str.buf));
    if (r < 0)
        return r;
    else if (r < LIBUSB_DT_HEADER_SIZE || str.desc.bLength > r)
        return LIBUSB_ERROR_IO;
    else if (str.desc.bDescriptorType != LIBUSB_DT_STRING)
        return LIBUSB_ERROR_IO;
    else if ((str.desc.bLength & 1) || str.desc.bLength != r)
        usbi_warn(HANDLE_CTX(dev_handle), "suspicious bLength %u for string descriptor (read %d)",
                  str.desc.bLength, r);

    di = 0;
    for (si = 2; si < str.desc.bLength; si += 2) {
        if (di >= (length - 1))
            break;

        wdata = libusb_le16_to_cpu(str.desc.wData[di]);
        if (wdata < 0x80)
            data[di++] = (unsigned char) wdata;
        else
            data[di++] = '?'; /* non-ASCII */
    }

    data[di] = 0;
    return di;
}

static int parse_iad_array(struct libusb_context *ctx,
                           struct libusb_interface_association_descriptor_array *iad_array,
                           const uint8_t *buffer, int size) {
    uint8_t i;
    struct usbi_descriptor_header header;
    int consumed = 0;
    const uint8_t *buf = buffer;
    struct libusb_interface_association_descriptor *iad;

    if (size < LIBUSB_DT_CONFIG_SIZE) {
        usbi_err(ctx, "short config descriptor read %d/%d",
                 size, LIBUSB_DT_CONFIG_SIZE);
        return LIBUSB_ERROR_IO;
    }

    // First pass: Iterate through desc list, count number of IADs
    iad_array->length = 0;
    while (consumed < size) {
        usbi_parse_descriptor(buf, "bb", &header);
        if (header.bDescriptorType == LIBUSB_DT_INTERFACE_ASSOCIATION)
            iad_array->length++;
        buf += header.bLength;
        consumed += header.bLength;
    }

    iad_array->iad = NULL;
    if (iad_array->length > 0) {
        iad = calloc(iad_array->length, sizeof(*iad));
        if (!iad)
            return LIBUSB_ERROR_NO_MEM;

        iad_array->iad = iad;

        // Second pass: Iterate through desc list, fill IAD structures
        consumed = 0;
        i = 0;
        while (consumed < size) {
            usbi_parse_descriptor(buffer, "bb", &header);
            if (header.bDescriptorType == LIBUSB_DT_INTERFACE_ASSOCIATION)
                usbi_parse_descriptor(buffer, "bbbbbbbb", &iad[i++]);
            buffer += header.bLength;
            consumed += header.bLength;
        }
    }

    return LIBUSB_SUCCESS;
}

static int raw_desc_to_iad_array(struct libusb_context *ctx, const uint8_t *buf,
                                 int size,
                                 struct libusb_interface_association_descriptor_array **iad_array) {
    struct libusb_interface_association_descriptor_array *_iad_array
            = calloc(1, sizeof(*_iad_array));
    int r;

    if (!_iad_array)
        return LIBUSB_ERROR_NO_MEM;

    r = parse_iad_array(ctx, _iad_array, buf, size);
    if (r < 0) {
        usbi_err(ctx, "parse_iad_array failed with error %d", r);
        free(_iad_array);
        return r;
    }

    *iad_array = _iad_array;
    return LIBUSB_SUCCESS;
}

/** \ingroup libusb_desc
 * Get an array of interface association descriptors (IAD) for a given
 * configuration.
 * This is a non-blocking function which does not involve any requests being
 * sent to the device.
 *
 * \param dev a device
 * \param config_index the index of the configuration you wish to retrieve the
 * IADs for.
 * \param iad_array output location for the array of IADs. Only valid if 0 was
 * returned. Must be freed with libusb_free_interface_association_descriptors()
 * after use. It's possible that a given configuration contains no IADs. In this
 * case the iad_array is still output, but will have 'length' field set to 0, and
 * iad field set to NULL.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
 * \returns another LIBUSB_ERROR code on error
 * \see libusb_get_active_interface_association_descriptors()
 */
int API_EXPORTED libusb_get_interface_association_descriptors(libusb_device *dev,
                                                              uint8_t config_index,
                                                              struct libusb_interface_association_descriptor_array **iad_array) {
    union usbi_config_desc_buf _config;
    uint16_t config_len;
    uint8_t *buf;
    int r;

    if (!iad_array)
        return LIBUSB_ERROR_INVALID_PARAM;

    usbi_dbg(DEVICE_CTX(dev), "IADs for config index %u", config_index);
    if (config_index >= dev->device_descriptor.bNumConfigurations)
        return LIBUSB_ERROR_NOT_FOUND;

    r = get_config_descriptor(dev, config_index, _config.buf, sizeof(_config.buf));
    if (r < 0)
        return r;

    config_len = libusb_le16_to_cpu(_config.desc.wTotalLength);
    buf = malloc(config_len);
    if (!buf)
        return LIBUSB_ERROR_NO_MEM;

    r = get_config_descriptor(dev, config_index, buf, config_len);
    if (r >= 0)
        r = raw_desc_to_iad_array(DEVICE_CTX(dev), buf, r, iad_array);

    free(buf);
    return r;
}

/** \ingroup libusb_desc
 * Get an array of interface association descriptors (IAD) for the currently
 * active configuration.
 * This is a non-blocking function which does not involve any requests being
 * sent to the device.
 *
 * \param dev a device
 * \param iad_array output location for the array of IADs. Only valid if 0 was
 * returned. Must be freed with libusb_free_interface_association_descriptors()
 * after use. It's possible that a given configuration contains no IADs. In this
 * case the iad_array is still output, but will have 'length' field set to 0, and
 * iad field set to NULL.
 * \returns 0 on success
 * \returns LIBUSB_ERROR_NOT_FOUND if the device is in unconfigured state
 * \returns another LIBUSB_ERROR code on error
 * \see libusb_get_interface_association_descriptors
 */
int API_EXPORTED libusb_get_active_interface_association_descriptors(libusb_device *dev,
                                                                     struct libusb_interface_association_descriptor_array **iad_array) {
    union usbi_config_desc_buf _config;
    uint16_t config_len;
    uint8_t *buf;
    int r;

    if (!iad_array)
        return LIBUSB_ERROR_INVALID_PARAM;

    r = get_active_config_descriptor(dev, _config.buf, sizeof(_config.buf));
    if (r < 0)
        return r;

    config_len = libusb_le16_to_cpu(_config.desc.wTotalLength);
    buf = malloc(config_len);
    if (!buf)
        return LIBUSB_ERROR_NO_MEM;

    r = get_active_config_descriptor(dev, buf, config_len);
    if (r >= 0)
        r = raw_desc_to_iad_array(DEVICE_CTX(dev), buf, r, iad_array);
    free(buf);
    return r;
}

/** \ingroup libusb_desc
 * Free an array of interface association descriptors (IADs) obtained from
 * libusb_get_interface_association_descriptors() or
 * libusb_get_active_interface_association_descriptors().
 * It is safe to call this function with a NULL iad_array parameter, in which
 * case the function simply returns.
 *
 * \param iad_array the IAD array to free
 */
void API_EXPORTED libusb_free_interface_association_descriptors(
        struct libusb_interface_association_descriptor_array *iad_array) {
    if (!iad_array)
        return;

    if (iad_array->iad)
        free((void *) iad_array->iad);
    free(iad_array);
}
