#include <stdio.h>

#define __STDC_CONSTANT_MACROS
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif

int main(int argc, char* argv[])
{
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *ifmt_ctx_v = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	int ret;
	int videoindex_v=-1,videoindex_out=-1;
	int frame_index=0;
	int64_t cur_pts_v=0;

	enum AVRounding avRounding;

	const char *in_filename_v = "../resource/aa.h264";
	const char *out_filename = "../resource/cdd.mp4";

	//The very start of the muxing
	av_register_all();

	//write basic info into input AVFormatContext(1)
	if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
		printf( "Could not open input file.");
		goto end;
	}
	//write basic info into input AVFormatContext(2)
	if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto end;
	}

	printf("===========Input Information==========\n");
	av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
	printf("======================================\n");
	
	//alloc memory for output AVFormatContext
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		printf( "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;

	//make sure the input stream is video
	if(ifmt_ctx_v->streams[0]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
		AVStream *in_stream = ifmt_ctx_v->streams[0];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		videoindex_v=0;
		if (!out_stream) {
			printf( "Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}
		videoindex_out=out_stream->index;
		//Copy the settings of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			printf( "Failed to copy context from input to output stream codec context\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
	}else{
		printf("Not a video file\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	printf("==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	printf("======================================\n");

	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
			printf( "Could not open output file '%s'", out_filename);
			goto end;
		}
	}

	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf( "Error occurred when opening output file\n");
		goto end;
	}

	//write in the content of video frame by frame
	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index=0;
		AVStream *in_stream, *out_stream;

		ifmt_ctx=ifmt_ctx_v;
		stream_index=videoindex_out;

		if(av_read_frame(ifmt_ctx, &pkt) >= 0){
			in_stream  = ifmt_ctx->streams[pkt.stream_index];
			out_stream = ofmt_ctx->streams[stream_index];

			if(pkt.stream_index==videoindex_v){
				//FIX：No PTS (Example: Raw H.264)
				//Simple Write PTS
				if(pkt.pts==AV_NOPTS_VALUE){
					//Write PTS
					AVRational time_base1=in_stream->time_base;
					//Duration between 2 frames (us)
					int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
					//Parameters
					pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
					pkt.dts=pkt.pts;
					pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
					frame_index++;
				}

				cur_pts_v=pkt.pts;
					
			}
		}else{
			break;
		}

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, avRounding);
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, avRounding);
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index=stream_index;

		printf("Write 1 Packet. size:%5d\tpts:%lld\n",pkt.size,pkt.pts);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf( "Error muxing packet\n");
			break;
		}
		av_free_packet(&pkt);

	}

	av_write_trailer(ofmt_ctx);

end:
	avformat_close_input(&ifmt_ctx_v);
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		printf( "Error occurred.\n");
		return -1;
	}
	return 0;
}
