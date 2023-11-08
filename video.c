#include "video.h"

 void calculateHistogram(unsigned char* image_data, int width, int height, int* histogram, int channel) {
    for (int i = 0; i < width * height; i++) {
        int pixel_value = image_data[i * 3 + channel]; // Adjust for Red, Green, or Blue channel
        histogram[pixel_value]++;
    }
}

void generateDataFile(const char* filename, int* histogram) {
    FILE* dataFile = fopen(filename, "w");
    if (!dataFile) {
        printf("Error opening the data file.\n");
        return;
    }

    for (int i = 0; i < 256; i++) {
        fprintf(dataFile, "%d %d\n", i, histogram[i]);
    }

    fclose(dataFile);
}

int openVideoFile(AVFormatContext **pFormatCtx, const char *filename) {
  // Open video file
  if (avformat_open_input(pFormatCtx, filename, NULL, NULL) != 0)
    return -1; // Couldn't open video file
 
  // Retrieve stream information
  if (avformat_find_stream_info(*pFormatCtx, NULL) < 0)
    return -1; // Couldn't find stream information
 
  return 0;
}
 
int findVideoStream(AVFormatContext *pFormatCtx, int *videoStream) {
  // Find the first video stream
  *videoStream = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      *videoStream = i;
      break;
    }
  }
  if (*videoStream == -1)
    return -1; // Didn't find a video stream
 
  return 0;
}
 
int openCodecContext(AVCodecContext **pCodecCtx, AVCodecContext **pCodecCtxOrig, AVCodec **pCodec, AVFormatContext *pFormatCtx, int videoStream) {
  // Get a pointer to the codec context for the video stream
  *pCodecCtxOrig = avcodec_alloc_context3(NULL);
  avcodec_parameters_to_context(*pCodecCtxOrig, pFormatCtx->streams[videoStream]->codecpar);
 
  // Find the decoder for the video stream
  *pCodec = avcodec_find_decoder((*pCodecCtxOrig)->codec_id);
  if (*pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }
 
  // Copy context
  *pCodecCtx = avcodec_alloc_context3(*pCodec);
  if (avcodec_copy_context(*pCodecCtx, *pCodecCtxOrig) != 0) {
    fprintf(stderr, "Couldn't copy codec context");
    return -1; // Error copying codec context
  }
 
  // Open codec
  if (avcodec_open2(*pCodecCtx, *pCodec, NULL) < 0)
    return -1; // Could not open codec
 
  return 0;
}
 
int saveFrame(AVFrame *pFrame, int width, int height, int iFrame,char *videoName) {
  // Save the frame to disk
  // Implementation not provided in the code snippet
  // You can write your own implementation to save the frame
   char szFilename[32];
  //int  y;
 
  // Open file
  videoName=strtok(videoName,".");
  sprintf(szFilename, "%sframe%d.jpg", videoName,iFrame);
  FILE *pFile = fopen(szFilename, "wb");
  if (pFile == NULL)
    return 0;
 
  // Create a JPEG compression object
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
 
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, pFile);
 
  // Set compression parameters
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 90, TRUE);
 
  // Start compression
  jpeg_start_compress(&cinfo, TRUE);
 
  // Write pixel data
  JSAMPROW row_pointer[1];
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &pFrame->data[0][cinfo.next_scanline * pFrame->linesize[0]];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
 
  // Finish compression
  jpeg_finish_compress(&cinfo);
 
  // Clean up
  jpeg_destroy_compress(&cinfo);
  fclose(pFile);
 
 
  return 0;
}
 
int decodeVideoFrames(AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx, int videoStream,char *videoName) {
struct SwsContext *sws_ctx = NULL;
  AVFrame *pFrame = av_frame_alloc();
  AVFrame *pFrameRGB = av_frame_alloc();
  if (pFrameRGB == NULL)
    return -1;
 
  // Determine required buffer size and allocate buffer
  int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);
  uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
 
  // Assign appropriate parts of buffer to image planes in pFrameRGB
  av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);
 
  // Initialize SWS context for software scaling
  sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                       pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24,
                                       SWS_BILINEAR, NULL, NULL, NULL);
 
  AVPacket packet;
  int frameFinished;
  int i = 0;
 
  // Read frames and save first five frames to disk
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream) {
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
 
      // Did we get a video frame?
      if (frameFinished) {
        // Convert the image from its native format to RGB
        sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                  pFrameRGB->data, pFrameRGB->linesize);
 
        // Save the frame to disk
        if (++i <= 5)
          saveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i,videoName);
      }
    }
 
    // Free the packet that was allocated by av_read_frame
    av_packet_unref(&packet);
  }
 
  // Free the RGB image
  av_freep(&buffer);
  av_frame_free(&pFrameRGB);
 
  // Free the YUV frame
  av_frame_free(&pFrame);
 
  return 0;
}
 
int closeVideoFile(AVFormatContext *pFormatCtx) {
  // Close the codecs
  avcodec_close(pFormatCtx->streams[0]->codec);
  avcodec_free_context(&pFormatCtx->streams[0]->codec);
 
  // Close the video file
  avformat_close_input(&pFormatCtx);
 
  return 0;
}
 
int main(int argc, char *argv[]) {
  AVFormatContext *pFormatCtx = NULL;
  AVCodecContext *pCodecCtx = NULL;
  AVCodecContext *pCodecCtxOrig = NULL;
  AVCodec *pCodec = NULL;
  int videoStream;
  char *videoName=argv[1];
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
    return -1;
  }
 
  // Register all formats and codecs
  av_register_all();
 
  // Open video file
  if (openVideoFile(&pFormatCtx, argv[1]) != 0)
    return -1;
 
  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, argv[1], 0);
 
  // Find the first video stream
  if (findVideoStream(pFormatCtx, &videoStream) != 0)
    return -1;
 
  // Open codec
  if (openCodecContext(&pCodecCtx, &pCodecCtxOrig, &pCodec, pFormatCtx, videoStream) != 0)
    return -1;
 
  // Decode video frames
  if (decodeVideoFrames(pFormatCtx, pCodecCtx, videoStream,videoName) != 0)
    return -1;
 
  // Close the video file
  if (closeVideoFile(pFormatCtx) != 0)
    return -1;
 //open the Currrent Directory
   DIR *dir = opendir(".");
    if (dir) 
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) 
        {
            if (entry->d_type == DT_REG) 
            { // Check if it's a regular file
                const char *name = entry->d_name;
                const char *file_extension = strrchr(name, '.');
                if (file_extension && strcmp(file_extension, ".jpg") == 0) 
                {
                    const char* jpeg_filename = name;
                   //open a jpg image file
                   // unsigned char* image_data = 
                     FILE* file = fopen(jpeg_filename, "rb");
    if (!file) {
        printf("Error opening the image file.\n");
        return 1;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int width = cinfo.output_width;
    int height = cinfo.output_height;

    int channels = 3;
    unsigned char* image_data = (unsigned char*)malloc(width * height * channels);
    unsigned char* row_pointer[1];
    row_pointer[0] = image_data;

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        row_pointer[0] += width * channels;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);

    int red_histogram[256] = {0};
    int green_histogram[256] = {0};
    int blue_histogram[256] = {0};

    for (int i = 0; i < channels; i++) {
        calculateHistogram(image_data, width, height, i == 0 ? red_histogram : i == 1 ? green_histogram : blue_histogram, i);
    }
 

    generateDataFile("red_data.txt", red_histogram);
    generateDataFile("green_data.txt", green_histogram);
    generateDataFile("blue_data.txt", blue_histogram);
    
    name=strtok(name,".");
    gnuplot_ctrl *h;
    
    h = gnuplot_init();
    gnuplot_cmd(h, "set terminal png ");
    gnuplot_cmd(h, "set output 'histograms%s.png'",name);
    gnuplot_cmd(h, "set title 'Channel Histograms'");
    gnuplot_cmd(h, "set xlabel 'Pixel Value'");
    gnuplot_cmd(h, "set ylabel 'Frequency'");
    gnuplot_cmd(h, "set style data histograms");
    gnuplot_cmd(h, "set style fill solid border 2");
    gnuplot_cmd(h, "set boxwidth 2 relative");
    gnuplot_cmd(h, "plot 'red_data.txt' using 1:2 with lines title 'Red Channel', \
                          'green_data.txt' using 1:2 with lines title 'Green Channel', \
                          'blue_data.txt' using 1:2 with lines title 'Blue Channel', \
                         ");
                 
    const char *file_name1="red_data.txt";
    const char *file_name2="green_data.txt";
    const char *file_name3="blue_data.txt";
    
    gnuplot_close(h);
    free(image_data);
    remove(file_name1);
    remove(file_name2);
    remove(file_name3);    
                }
            }
        }
        closedir(dir);
    }
    else 
    {
        perror("Error opening directory");
    }
 
  return 0;
}
