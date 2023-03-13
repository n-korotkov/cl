import sys
import wikipediaapi
wiki = wikipediaapi.Wikipedia('ru')

with open(sys.argv[1], 'r') as list_file:
    for line in list(list_file):
        article_name = line[:-1]
        print(f'{article_name}: ', end='', flush=True)
        wiki_page = wiki.page(article_name)
        article_file_name = article_name.translate(str.maketrans('<>:"/\|?* ', '__________'))
        with open(f'{article_file_name}.txt', 'w') as article_file:
            article_file.write(wiki_page.text)
        print('Done')
