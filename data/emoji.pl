#!/usr/bin/env perl
# Writes data/emoji.txt, the emoji the notebook emoji picker offers: each single character
# with the Emoji property, then a tab and its Unicode name in lowercase. Characters drawn as
# text by default get U+FE0F so they show in color. Faces come first, then pictures, then
# symbols, much as an emoji keyboard lists them. Run from the repository root:
#
#   perl data/emoji.pl > data/emoji.txt
use strict;
use warnings;
use charnames ();
use Unicode::UCD ();

binmode STDOUT, ':encoding(UTF-8)';
printf "# Unicode %s emoji, from data/emoji.pl.\n", Unicode::UCD::UnicodeVersion();
my @ranges = ([0x1F600, 0x1F64F], [0x1F900, 0x1FAFF], [0x1F300, 0x1F5FF], [0x1F680, 0x1F6FF],
              [0x1F000, 0x1F2FF], [0x1F650, 0x1F67F], [0x1F700, 0x1F8FF], [0xA9, 0x1EFFF]);
for my $code (map { $_->[0] .. $_->[1] } @ranges) {
    my $char = chr $code;
    next unless $char =~ /\p{Emoji}/ && $char !~ /\p{Emoji_Component}/;
    my $name = charnames::viacode($code) or next;
    $char .= "\x{FE0F}" unless $char =~ /\p{Emoji_Presentation}/;
    printf "%s\t%s\n", $char, lc $name;
}
