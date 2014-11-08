#!/usr/bin/perl -w
use Carp 'croak';
use Imager::Screenshot 'screenshot';
use Time::HiRes 'sleep';
use Const::Fast;

const my %constant=>(max_duration=>10,      # sec
   out_name=>'/tmp/test.gif', fps=>13,      # output filename, frame per sec
   resize_option=>'--resize-fit 300x300 --resize-method lanczos3');
const my $delay=>1/$constant{fps};    # in unit of msec
const my $max_frames=>$constant{fps}*$constant{max_duration};
my @frames; my $interrupt=0;
$SIG{INT}=sub{$interrupt=1};          # installs interrupt signal handler
sub main;
sub post_process;
exit !main;

sub capture{
   my ($left, $right, $top,$bottom)=@_;
   Imager->set_file_limits(height=>200,bytes=>2_000_000);
   for(0..$max_frames){
	push @frames, screenshot(left=>$left, right=>$right, top=>$top, bottom=>$bottom);
	last if $interrupt;
	sleep $delay
   }
   Imager->write_multi({file => $constant{out_name},
	   type => 'gif',
	   gif_loop => 0, # loop forever
	   gif_delay => $delay*100,              # in unit of 10ms
	   gif_interlace =>1
	}, @frames)
	or croak "Cannot write $constant{out_name}: ", Imager->errstr, "\n";
   post_process
}

sub post_process{                     # resize, optimize
   system qq(gifsicle -O3 $constant{resize_option} $constant{out_name} > $constant{out_name}.'1');
   system qq(mv $constant{out_name}.'1' $constant{out_name});
   `gifview -a $constant{out_name}`
}

sub main{
   croak "Usage: $0 left right top bottom" unless @ARGV==4;
   capture @ARGV;
}
