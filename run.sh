task="./$1"
shift
$task corpus/articles dictionaries/dict.opcorpora.xml dictionaries/freqrnc2011.csv "$@"
