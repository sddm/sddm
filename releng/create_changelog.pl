#!/usr/bin/perl
#############################################################################
##
## Copyright (C) 2013 Intel Corporation.
## Contact: http://www.qt-project.org/legal
##
## $QT_BEGIN_LICENSE:BSD$
## You may use this file under the terms of the BSD license as follows:
##
## "Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##   * Redistributions of source code must retain the above copyright
##     notice, this list of conditions and the following disclaimer.
##   * Redistributions in binary form must reproduce the above copyright
##     notice, this list of conditions and the following disclaimer in
##     the documentation and/or other materials provided with the
##     distribution.
##   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
##     of its contributors may be used to endorse or promote products derived
##     from this software without specific prior written permission.
##
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
##
## $QT_END_LICENSE$
##
#############################################################################

use strict;
use warnings;
use FileHandle;
use Getopt::Long;
use Text::Wrap;
my %log;
my $optVerbose = 0;

sub help {
    print "Usage: create-changelog [OPTIONS] <srcdir> <revision-range>\n" .
        "\n" .
        "srcdir                  the qt source directory\n" .
        "revision-range          arguments to git log, like v5.0.0..v5.1.0-rc1\n" .
        "Options:\n" .
        "  -v                    Verbose output.\n";
    exit 0;
}

sub isMasterRepo
{
    my $modulesFileName = '.gitmodules';
    return 0 unless -e $modulesFileName;
    my $modulesFile = new IO::File('<' . $modulesFileName) or return 0;
    while (my $line = <$modulesFile>) {
        if ($line =~ /\[submodule\s*"qtbase"\]/) {
            $modulesFile->close();
            return 1;
        }
    }
    $modulesFile->close();
    return 0;
}

sub collect_entries {
    # Run git submodule foreach
    chdir(shift @ARGV) if (scalar @ARGV);
    my $cmd = "git rev-list --reverse --grep '^\\[ChangeLog\\]' " . $ARGV[0];
    $cmd .= ' 2> /dev/null' unless $optVerbose;
    $cmd .= ' | git cat-file --batch || true';
    my @revListCommand = ($cmd);
    unshift(@revListCommand, 'git', 'submodule', 'foreach', '--quiet') if isMasterRepo();
    print STDERR "Running: ", join(' ', @revListCommand), "\n";
    open FOREACH, '-|', @revListCommand;

    # Collect all entries
    while (<FOREACH>) {
        /(^[0-9a-f]{40}) commit (\d+)/
            or die("Could not parse header line: $_");
        my %entry;
        $entry{commit} = $1;

        my $msg;
        die("Could not read log message")
            unless read(FOREACH, $msg, $2 + 1) == $2 + 1;

        # is there a task number?
        # Extract each argument after some keywords to an array
        my @tasks = ( $msg =~ /^(?:Issues|Closes|Fixes):\s*(.*)\s*$/mixg );
        $entry{tasks} = \@tasks;

        # Extract the changelogs from $msg
        my @texts = ( $msg =~ /\[ChangeLog\](.*?)\n(?=\n)/sixg );
        foreach (@texts) {
            my @groups;
            if (/\[((?:[^]]|\]\[)+)\]\s*(.*)\z/si) {
                @groups = split(/\]\[/, $1);
                $entry{text} = $2 =~ s/\s+/ /gr;
                $entry{text} =~ s/\s+$//;
            } else {
                warn('Malformed line: "' . $_ . "\"\n\n");
                push(@groups, 'UNSPECIFIED');
                $entry{text} = $_;
            }

            # Store this entry
            # Each entry in %log is a hash
            my $topgroup = shift @groups;
            my $logentry = \$log{$topgroup};
            $$logentry = {} unless defined($$logentry);

            my $subentry;
            if (scalar @groups) {
                # Two-level entry
                $subentry = join(' / ', @groups);
            } else {
                # One-level entry
                $subentry = 0;
            }
            my $array = \$$$logentry{$subentry};
            $$array = [] unless defined($$array);
            push @{ $$array }, { %entry };
        }
    }
    close FOREACH or die("git submodule foreach died: $!");
}

sub print_entry($%) {
    my $level = $_[0];
    my %entry = %{$_[1]};
    die if $level > 1;

    my $line;
    #$line = $entry{commit};
    $line .= join('', map { "[$_]" } @{$entry{tasks}});
    $line .= ' ' if scalar @{$entry{tasks}};
    $line .= $entry{text};
    if ($level == 0) {
        print wrap(" - ", "   ", $line);
    } else {
        print wrap("   * ", "     ", $line);
    }
    print "\n";
}

help() unless GetOptions("verbose" => \$optVerbose) && scalar(@ARGV) == 2;

# Now print the output
collect_entries();
for my $toplevel (sort keys %log) {
    print "\n$toplevel\n";
    print '-' x (length $toplevel) . "\n";

    my $value = \$log{$toplevel};

    # Print one-level entries first
    my $entry = \$$$value{0};
    if (defined($$entry)) {
        print "\n";
        foreach my $subentry (@$$entry) {
            print_entry(0, $subentry);
        }
    }

    # Print two-level entries now
    foreach my $sublevel (sort keys %{ $$value }) {
        next if $sublevel eq "0";
        print "\n - $sublevel:\n";
        $entry = \$$$value{$sublevel};
        foreach my $subentry (@$$entry) {
            print_entry(1, $subentry);
        }
    }
}
