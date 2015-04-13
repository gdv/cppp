#!/usr/bin/env ruby
# coding: utf-8

# Copyright 2015
# Gianluca Della Vedova <http://gianluca.dellavedova.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This program receives a matrix (option -m) and a phylogeny (option -p)
# in Newick format and checks if the phylogeny realizes the matrix.
# The matrix has a species for each row and a character for each column.
# Each entry M[s,c] can be:
# 0: species s does not possess character c
# 1: species s possesses character c
# 2: species s does not possess character c, and neither any ancestor of s possesses c
#
# The program returns 0 and produces no output if the phylogeny realizes
# the matrix, otherwise it returns 1 and outputs at least a species and a character
# that are not realized correctly

require 'fileutils'
require 'optparse'
require 'optparse/time'
require 'ostruct'
require 'pp'
require 'matrix'
require 'set'
require 'parslet'
include Parslet
require 'pry'

class OptparseExample
  def self.parse(args)
    # The options specified on the command line will be collected in *options*.
    # We set default values here.
    options = OpenStruct.new
    options.library = []
    options.inplace = false
    options.encoding = "utf8"
    options.transfer_type = :auto
    options.verbose = false

    opt_parser = OptionParser.new do |opts|
      opts.banner = "Usage: convert.rb [options]"

      opts.separator ""
      opts.separator "Specific options:"


      # Mandatory argument.
      opts.on("-m", "--matrix MATRIX_FILENAME",
              "Input matrix") do |matrix|
        options.matrix = matrix
      end

      # Mandatory argument.
      opts.on("-p", "--phylogeny PHYLOGENY_FILENAME",
              "Input phylogeny") do |phylogeny|
        options.phylogeny = phylogeny
      end


      # No argument, shows at tail.  This will print an options summary.
      # Try it and see!
      opts.on_tail("-h", "--help", "Show this message") do
        puts opts
        exit
      end
    end
    opt_parser.parse!(args)
    options
  end
end

options = OptparseExample.parse(ARGV)

class LabeledMatrix
  attr_reader :s_num, :c_num, :s_labels, :c_labels, :matrix
  # :matrix is the hash of columns, where the keys are the character names and each value
  # is a column that is represented as a string
  # The column ordering is the lexicographic order of the strings

  def pp
    puts @s_num
    puts @c_num
    puts #{"@matrix}"
    delimiter = @c_labels.map { |c| '_' }.join('')
    @c_labels[0].length.times do |i|
      puts '      ' + @c_labels.map { |c| c[i] }.join('')
    end
    puts "______#{delimiter}"
    @s_num.times do |i|
      puts "#{s_labels[i]}:#{@c_labels.map { |c| @matrix[c][i] }.join('')}"
    end
    puts "______#{delimiter}"
  end

  def s_label(num)
    return "s%04d" % num
  end

  def c_label(num)
    if @persistent
      sign = num % 1
      root = (num - sign) / 2 + 1
      if sign == 0
        return "C%04d+" % root
      else
        return "C%04d-" % root
      end
    else
      return "C%05d" % (num + 1)
    end
  end


  def initialize(arr, persistent_chars)
    @persistent = persistent_chars
    m = arr.map { |r| r.chomp }
    @c_labels = Array.new
    if m[0][0] == "\#"
      # The first line contains the optional list of characters, separated
      # by ,;
      header = m[0][1..-1]
      @c_labels = header.split(/[,;]/)
    end
    @c_num = m[0].length
    @s_num = m.length

    @matrix = Hash.new()
    m[0].split('').each_with_index { |x, idx| @matrix[c_label(idx)] = String.new("") }
    m.map do |row|
      row.chomp.split('').map.with_index { |x, idx| @matrix[c_label(idx)] << x }
    end

    @c_labels = @matrix.keys unless @c_labels.length > 0
    @s_labels = m.map.with_index { |x, i| s_label(i) }
    @s_chars  = m[0].split('').map.with_index   { |x, i| c_label(i) }
  end

  def swap_columns(i, j)
    @c_label[i], @c_label[j] = @c_label[j], @c_label[i]
    @matrix[i], @matrix[j] = @matrix[j], @matrix[i]
  end

  # Sort columns in decreasing lexicographic order
  def sort_columns
    @c_labels = Array.new(@matrix.keys)
    @c_labels.sort! { |a,b| @matrix[b] <=> @matrix[a] }
  end

  def remove_null
    @matrix.each_pair do |c, chars|
      if chars =~ /^0+$/
        @matrix.delete(c)
        @c_labels.delete(c)
      end
      @c_num = @matrix.keys.length
    end
  end

  def species(character)
    # Output the list of species that have the input character
    column = @matrix[character]
    return column.length.times.select { |p| column[p] == '1'}
  end

  def characters(species)
    # Output the list of characters that the input species possesses
    return @matrix.keys.select { |k| @matrix[k][species] == '1' }
  end

  # Considering only the columns corresponding to the input characters,
  # find the list of maximal characters
  def find_maximal_chars(all_characters)
    maximals = Array.new()
    while not all_characters.empty?
      maximals.push(all_characters.first)
      species_first_char = species(all_characters.first)
      block_chars = species_first_char.map { |s| characters(s) }
      union = block_chars.inject([]) { |old, new| old.concat(new) }.uniq
      union.map { |c|  all_characters.delete(c) }
    end
    return maximals
  end

  # Partition the characters in the portions starting with the maximal characters
  # i.e. each maximal characters begins a new portion
  def partition(characters_set)
    # Copy the characters_set array, otherwise it is passed as reference
    maximals = find_maximal_chars(Marshal.load(Marshal.dump(characters_set)))
    classes = Array.new
    # For each maximal character m, get the species that have m
    # If the matrix has a perfect phylogeny, the union of the characters possessed
    # by those species is a class of the partition
    maximals.map { |m| species(m) }.each do |species_set|
      char_set = species_set.map.inject(Set.new) { |s, x| s.union(characters(x).to_set) }.intersection(characters_set.to_set)
      char_array = char_set.to_a
      # puts "characters_set: #{characters_set}"
      # puts "maximals: #{maximals}"
      # puts "char_array: #{char_array}"
      classes.push(char_array.sort { |a,b| characters_set.index(a) <=> characters_set.index(b) } )
    end
    return classes
  end
end

class PersistentSpecies
  attr_reader :realized, :persistent
  def initialize(realized, persistent)
    @persistent = Set.new(persistent)
    @realized = Set.new(realized).subtract(@persistent)
  end
end

# Gets the parse tree of a Newick trees
# returns a set of Persistentspecies, one for each species
def visit_tree(newick)
  #  binding.pry
  if newick.kind_of?(Array)
    # The current node has at least two children
    # The set of paths is the union over all children
    newick.map { |subtree| visit_tree(subtree) }.inject(Set.new) { |result, set| result.union(set) }
  else
    # Only one child
    subtree_paths = [ PersistentSpecies.new(Set.new, Set.new) ]
    if newick.key? :subtree
      # There is a subtree
      subtree_paths.concat(visit_tree(newick[:subtree]))
    end
    # Add the current edge label to all paths
    if newick[:sign].nil? or newick[:sign] == '+'
      subtree_paths.map { |path| PersistentSpecies.new(path.realized.add("C%05d" % newick[:parsed_character][:int].to_i), path.persistent) }
    else
      subtree_paths.map { |path| PersistentSpecies.new(path.realized, path.persistent.add("C%05d" % newick[:parsed_character][:int].to_i)) }
    end
  end
end

module Newick
  # Newick format parser
  class Parser < Parslet::Parser
    rule(:tree) { subtree.as(:parsed) >> str(';') }

    # Separators
    rule(:lparen)     { space? >> str('(') >> space? }
    rule(:rparen)     { space? >> str(')') >> space? }
    rule(:comma)      { space? >> str(',') >> space? }
    rule(:colon)      { space? >> str(':') >> space? }
    rule(:space)      { match('\s').repeat(1) }
    rule(:space?)     { space.maybe }

    # Character classes
    rule(:integer)    { match('[0-9]').repeat(1).as(:int)  }
    rule(:sign)       { match('[+-]').maybe.as(:sign)  }

    # Labels
    rule(:species)    { str('S')  >> integer >> space? }
    rule(:species?)   { species.maybe }
    rule(:character)  { str(':C') >> integer >> sign >> space? }
    rule(:fields)     { species?.as(:parsed_species) >> character.as(:parsed_character) }
    rule(:label)      { fields }
    # Tree structure
    rule(:subtree)    { lparen >> (leaf | branching ) >> rparen }
    rule(:leaf)       { label }
    rule(:branching)  { branch >> (comma >> branch).repeat }
    rule(:branch)     { subtree.as(:subtree) >> label }

    root :tree
  end

  class Transformer < Parslet::Transform
    rule(:sign => simple(:sign)) { Integer(x.to_i) }
  end

  def self.parse(s)
    parser = Parser.new
    transformer = Transformer.new

    tree = parser.parse(s)
    out = transformer.apply(tree)

    #    out
    tree
  rescue Parslet::ParseFailed => failure
    puts failure.cause.ascii_tree
  end
end

m = LabeledMatrix.new(File.readlines(options.matrix), false)
m.remove_null


tree_str = IO.read options.phylogeny
tree = Newick.parse(tree_str.chomp)

#binding.pry

# compute the set of characters of each species in the matrix
from_matrix = m.s_num.times.map { |s| m.characters(s).to_set }.to_set
# compute the set of paths for the paths from the root to each node of the tree
# Notice that some of those paths might not correspond to a species
from_tree = visit_tree(tree[:parsed]).to_a.map { |path| path.realized }.to_set

# Check that all species can be realized in the tree
unless from_matrix <= from_tree
  puts "A species could not be realized"
  not_found = from_matrix - from_tree
  m.s_num.times.map do |s|
    if not_found.include?(m.characters(s).to_set)
      puts "Species #{m.s_label(s)}, with characters #{m.characters(s).to_s} could not be realized"
    end
  end
end
