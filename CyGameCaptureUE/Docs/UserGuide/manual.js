/* Offline enhancement only: all chapters and language links work without JavaScript. */
(() => {
  'use strict';
  document.body.classList.add('enhanced');
  const search = document.querySelector('#search');
  const sidebar = document.querySelector('#sidebar');
  const menu = document.querySelector('#menu');
  const sections = [...document.querySelectorAll('main section')];
  const normalize = text => text.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase();
  const corpus = new Map(sections.map(section => [section, normalize(section.textContent)]));
  const results = document.querySelector('#results');
  const applySearch = () => {
    const terms = normalize(search.value).trim().split(/\s+/).filter(Boolean);
    let count = 0;
    for (const section of sections) {
      section.hidden = !terms.every(term => corpus.get(section).includes(term));
      sidebar.querySelector(`a[href="#${section.id}"]`).hidden = section.hidden;
      if (!section.hidden) count++;
    }
    results.textContent = terms.length ? results.dataset.template.replace('{n}', count) : '';
    document.querySelector('#empty').hidden = count !== 0;
  };
  document.querySelector('#search-box').hidden = false;
  search.addEventListener('input', applySearch);
  menu.hidden = false;
  menu.addEventListener('click', () => {
    const open = sidebar.classList.toggle('open');
    menu.setAttribute('aria-expanded', String(open));
  });
  sidebar.addEventListener('click', event => {
    if (event.target.closest('a')) {
      sidebar.classList.remove('open');
      menu.setAttribute('aria-expanded', 'false');
    }
  });
  for (const link of document.querySelectorAll('a.language')) {
    link.addEventListener('click', () => { link.hash = location.hash; });
  }
  const print = document.querySelector('#print');
  print.hidden = false;
  print.addEventListener('click', () => window.print());
  const revealTarget = () => {
    const section = document.getElementById(location.hash.slice(1));
    if (section && section.hidden) {
      search.value = '';
      applySearch();
      section.scrollIntoView();
    }
  };
  window.addEventListener('hashchange', revealTarget);
  revealTarget();
})();
