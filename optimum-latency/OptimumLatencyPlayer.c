/* 
 * Created (11/4/2019) by Paolo-Pr.
 * This file is part of Live Asynchronous Audio Video Library.
 *
 * Live Asynchronous Audio Video Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Live Asynchronous Audio Video Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Live Asynchronous Audio Video Library.  If not, see <http://www.gnu.org/licenses/>.
 * --------------------------------------------
 *
 * This example plays the audio+video stream provided by OptmimumLatencyStreamer.
 * See README.md included in this directory.
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <unistd.h>

typedef struct _custom_data {
    GstElement *sink_pipeline;
    GstElement *udp_source;
    GstElement *audio_source_pipeline;
    GstElement *video_source_pipeline;  
    GstElement *udp_sink;
    GstElement *audio_demuxer;
    GstElement *video_demuxer;  
    GstElement *video_parser;
    GstElement *audio_decoder;
    GstElement *video_decoder;  
    GstElement *audio_converter;      
    GstElement *audio_sink;   
    GstElement *video_sink;
} custom_data;

void print_curr_date()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    char day[4], mon[4];
    int wday, hh, mm, ss, year;
    sscanf(ctime((time_t*) &(ts.tv_sec)), "%s %s %d %d:%d:%d %d",
            day, mon, &wday, &hh, &mm, &ss, &year);
    fprintf(stderr, "[ %s %s %d %d:%d:%d %ld ]", day, mon, wday, hh, mm, ss, ts.tv_nsec);
}

int pcr_found = 0;

static GstFlowReturn
on_new_sample_from_sink(GstElement *elt, custom_data *data)
{
    GstSample *sample;
    GstBuffer *app_buffer_1, *app_buffer_2, *buffer;
    GstElement *source;
    GstElement *source2;  
    GstFlowReturn ret;

    // get the sample from appsink
    sample = gst_app_sink_pull_sample(GST_APP_SINK(elt));
    buffer = gst_sample_get_buffer(sample);

    // make a copy
    app_buffer_1 = gst_buffer_copy(buffer);
    app_buffer_2 = gst_buffer_copy(buffer);

    // we don't need the appsink sample anymore
    gst_sample_unref(sample);
    
    // get source an push new buffer  
    if (pcr_found)  
    {
        source = gst_bin_get_by_name(GST_BIN(data->audio_source_pipeline), "udpaudiosource");
        ret = gst_app_src_push_buffer(GST_APP_SRC(source), app_buffer_1); 
        gst_object_unref(source);
        source = gst_bin_get_by_name(GST_BIN(data->video_source_pipeline), "udpvideosource");
        ret = gst_app_src_push_buffer(GST_APP_SRC(source), app_buffer_2);
        gst_object_unref(source);
    }
    else
        ret = GST_FLOW_OK;

    return ret;
}

static GstPadProbeReturn
cb_have_data(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    
    gint x, y;
    GstMapInfo map;
    guint16 *ptr, t;
    GstBuffer *buffer;
    const char **data_info =(const char **)user_data;

    buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    buffer = gst_buffer_make_writable(buffer);

    if (!gst_buffer_map(buffer, &map, GST_MAP_READ) ) 
    {
        g_printerr("gst_buffer_map() failed...\n");
        exit(1);
    }

    if (buffer == NULL)
        return GST_PAD_PROBE_OK;

    GstClockTime timestamp = GST_BUFFER_PTS(buffer);

    int q;

    if (!pcr_found)
    for(q  = 0; q < gst_buffer_get_size(buffer) ; q = q+188)
    {
        //MPEG PES?
        if ((int)map.data[0+q] == 71 && (int)map.data[1+q] == 65 && (int)map.data[2+q] == 0) 
        {
            // have AF?
            if (( map.data[3+q] & 0x20 ) &&( map.data[4+q] > 0 ) ) 
                //have pcr?
                if ((int)map.data[5+q] & 80 ) 
                    pcr_found = 1;
        }  
    }

    if (data_info != NULL)
    {
        print_curr_date();
        unsigned long int size = gst_buffer_get_size(buffer);
        // gstreamer adds extra data to video demuxed frames
        if (!strcmp("[VIDEO] Demuxed data  ", *data_info) && size >= 6)
            size -= 6;
        g_printerr(" %s: size=%" G_GSIZE_FORMAT " ts=%" G_GUINT64_FORMAT "\n", 
                *data_info, size, timestamp);     
    }
    
    GST_PAD_PROBE_INFO_DATA(info) = buffer;
 
    gst_buffer_unmap (buffer, &map);
    
    return GST_PAD_PROBE_OK;
}

int
main(int argc, char *argv[])
{
    custom_data data;    
    GstMessage *msg;
    GstBus *bus;
    GError *error = NULL;

    gst_init(&argc, &argv);

    data.sink_pipeline = gst_parse_launch 
    
    //You can try also to set the properties "blocksize and buffer-size" to small values on udpsrc
    ("udpsrc port=8084 name=udpsrc ! appsink name=udpsink emit-signals=true sync=false", 
     &error);
    
    data.audio_source_pipeline = gst_parse_launch 
    ("appsrc name=udpaudiosource ! tsdemux name=audiodemuxer ! avdec_opus name=audiodecoder ! audioconvert name=audioconverter ! pulsesink sync=false async=false latency-time=5000", 
     &error);    
     
    data.video_source_pipeline = gst_parse_launch 
    ("appsrc name=udpvideosource ! tsdemux name=videodemuxer ! h264parse name=videoparser "
     "! avdec_h264 name=videodecoder max-threads=1 "
     "! xvimagesink sync=false async=false max-lateness=10000",
     &error);  

    if (!data.sink_pipeline) 
    {
        g_printerr("Parse error: %s\n", error->message);
        exit(1);
    }

    //gst_pipeline_set_latency(GST_PIPELINE(data.sink_pipeline), 10000 /*20000000*/);//10ms
        
    data.udp_sink = gst_bin_get_by_name(GST_BIN(data.sink_pipeline), "udpsink");
    g_signal_connect(data.udp_sink, "new-sample",
                    G_CALLBACK(on_new_sample_from_sink), &data);
    gst_object_unref(data.udp_sink);

    GstPad* pad;

    data.udp_source = gst_bin_get_by_name(GST_BIN(data.sink_pipeline), "udpsrc");    
    const char *data_source_0 = "[UDPSRC]              ";   
    pad = gst_element_get_static_pad(data.udp_source, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                      (GstPadProbeCallback) cb_have_data, NULL, NULL);
    gst_object_unref(pad);
    gst_object_unref(data.udp_source);    

    data.audio_decoder = gst_bin_get_by_name(GST_BIN(data.audio_source_pipeline), "audiodecoder");  
    
    const char *data_source_1 = "[AUDIO] Demuxed data  ";  
    pad = gst_element_get_static_pad(data.audio_decoder, "sink");  
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                      (GstPadProbeCallback) cb_have_data, &data_source_1, NULL);
    gst_object_unref(pad);    
    
    const char *data_source_2 = "[AUDIO] Decoded data  ";   
    pad = gst_element_get_static_pad(data.audio_decoder, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                       (GstPadProbeCallback) cb_have_data, &data_source_2, NULL);
    gst_object_unref(pad);
    gst_object_unref(data.audio_decoder);
    
    data.audio_converter  = gst_bin_get_by_name(GST_BIN(data.audio_source_pipeline),  "audioconverter");   
    
    const char *data_source_3 = "[AUDIO] Converted data";   
    pad = gst_element_get_static_pad(data.audio_converter, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                       (GstPadProbeCallback) cb_have_data, &data_source_3, NULL);
    gst_object_unref(pad);
    gst_object_unref(data.audio_converter);     
    
    data.video_parser = gst_bin_get_by_name(GST_BIN(data.video_source_pipeline),  "videoparser");    
    
    const char *data_source_4 = "[VIDEO] Demuxed data  ";  
    pad = gst_element_get_static_pad(data.video_parser, "sink");  
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                       (GstPadProbeCallback) cb_have_data, &data_source_4, NULL);
    gst_object_unref(pad);
    
    const char *data_source_5 = "[VIDEO] Parsed data   ";  
    pad = gst_element_get_static_pad(data.video_parser, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                       (GstPadProbeCallback) cb_have_data, &data_source_5, NULL);
    gst_object_unref(pad);
    gst_object_unref(data.video_parser);    

    data.video_decoder = gst_bin_get_by_name(GST_BIN(data.video_source_pipeline),  "videodecoder");    
    
    const char *data_source_6 = "[VIDEO] Decoded data  ";   
    pad = gst_element_get_static_pad(data.video_decoder, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                       (GstPadProbeCallback) cb_have_data, &data_source_6, NULL);
    gst_object_unref(pad);
    gst_object_unref(data.video_decoder);
    
    gst_element_set_state(data.sink_pipeline, GST_STATE_PLAYING);
    gst_element_set_state(data.audio_source_pipeline, GST_STATE_PLAYING);
    gst_element_set_state(data.video_source_pipeline, GST_STATE_PLAYING);

    bus = gst_element_get_bus(data.sink_pipeline);
    msg = gst_bus_poll(bus, GST_MESSAGE_EOS | GST_MESSAGE_ERROR, -1);

    switch (GST_MESSAGE_TYPE(msg)) 
    {
        case GST_MESSAGE_EOS: 
        {
            g_print("EOS\n");
            break;
        }
        case GST_MESSAGE_ERROR: 
        {
            GError *err = NULL;
            gchar *dbg = NULL;

            gst_message_parse_error(msg, &err, &dbg);
            if (err) 
            {
                g_printerr("ERROR: %s\n", err->message);
                g_error_free(err);
            }
            if (dbg) 
            {
                g_printerr("[Debug details: %s]\n", dbg);
                g_free(dbg);
            }
        }
        default:
            g_printerr("Unexpected message of type %d", GST_MESSAGE_TYPE(msg));
            break;
    }

    gst_element_set_state(data.sink_pipeline, GST_STATE_NULL);
    gst_element_set_state(data.audio_source_pipeline, GST_STATE_NULL);
    gst_element_set_state(data.video_source_pipeline, GST_STATE_NULL);
    
    gst_message_unref(msg);
    gst_object_unref(bus); 
    
    gst_object_unref(data.sink_pipeline);
    gst_object_unref(data.video_source_pipeline);
    gst_object_unref(data.audio_source_pipeline); 
    
    return 0;
}
