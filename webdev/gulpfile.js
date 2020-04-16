const del = require('delete');
const through2 = require('through2');
const { series, parallel, src, dest, tree } = require('gulp');
const gzip = require('gulp-gzip');
const sass = require('gulp-dart-sass');
const prep = require('gulp-preprocess');
const gulpif = require('gulp-if');
const rename = require('gulp-rename');
const htmlmin = require('gulp-htmlmin');

const fs = require('fs');
const path = require('path');

var srcdir = path.resolve(__dirname);
var dstdir = path.resolve(__dirname, '..', 'dist');
var build = path.resolve(__dirname, 'build');
var context = { NODE_ENV: 'production', DEBUG: false };
var files = {};
try {
    let json = fs.readFileSync(path.join(__dirname, 'movefile.json'));
    files = JSON.parse(json);
} catch(e) {
    console.error(e);
}

function relpath(p, base=__dirname) {
    return path.relative(base, p)
}

function logfile(v) {
    console.log('file:', relpath(v.path))
}

function getfile(p) {
    if (p.hasOwnProperty('cwd')) p = p.path;
    p = relpath(p, srcdir);
    if (files.hasOwnProperty(p)) return files[p];
}

function mapname(p) {
    let file = getfile(p);
    if (!file) return;
    let paths = path.parse(file.dst);
    return {
        dirname: paths.dir,
        basename: paths.name,
        extname: paths.ext + (file.compress ? '.gz' : '')
    }
}

function sassSource() {
    return src(['**/*.{scss,sass}', '!node_modules/**'])
        .pipe(sass.sync({
            outputStyle: ['expanded', 'compressed'][0],
            sourceMap: false, indentType: 'space', indentWidth: 4
        }).on('error', sass.logError).on('data', logfile))
        .pipe(dest(build));
}

function prepSource() {
    let fns = Object.keys(files)
        .filter(file => file.endsWith('.html') && !file.includes('sitemap'))
        .filter(fs.existsSync);
    return src(fns, { cwdbase: true })
        .pipe(prep({context}))
        .pipe(dest(build).on('data', logfile));
}

function sitemap(cb) {
    let template = Object.keys(files).filter(f => f.includes('sitemap'));
    if (!template.length) return cb();
    let sources = [];
    let load = () => src('**/*.html*', { cwd: dstdir, read: false })
        .pipe(through2.obj((v, _, cb) => {
            let file = relpath(v.path, v.cwd),
                link = file.replace('.gz', '')
                    .replace('root/', '/')
                    .replace('src/', 'assets/');
            console.log(`Site page ${file} -> link ${link}`);
            sources.push(`<li><a href="${link}">${link}</a></li>`);
            cb(null, v);
        }));
    let merge = cb => Object.assign(context, {
        LINKS: sources.toString(), INFO: false,
    }) && cb();
    let dump = () => src(template[0], { allowEmpty: true, cwdbase: true })
        .pipe(rename(mapname(template[0])))
        .pipe(prep({context}))
        .pipe(dest(dstdir).on('data', logfile));
    return series(load, merge, dump)(cb);
}

function moveSource() {
    let exists = Object.keys(files).reduce((arr, file) => {
        let guess = [
            path.join(build, file), path.join(srcdir, file)
        ].filter(fs.existsSync);
        if (guess.length) arr.push(relpath(guess[0]));
        else console.error(`Source file ${file} not found. Skip`);
        return arr;
    }, []);
    let htmlminify = htmlmin({
        minifyCss: true, minifyJS: true,
        processScripts: ['text/x-template'],
        continueOnParseError: true,
    });
    let compress = v => (file = getfile(v.path)) ? file.compress : false;
    return src(exists, { cwdbase: true })
        .pipe(rename((pathobj, v) => {
            if (v.path.startsWith(build)) {
                pathobj.dirname = path.dirname(relpath(v.path, build));
            }
        }))
        .pipe(gulpif(v => v.extname === '.html' && compress(v), htmlminify))
        .pipe(gulpif(v => compress(v), gzip()))
        .pipe(rename((pathobj, v) => mapname(v.path.replace('.gz', ''))))
        .pipe(dest(dstdir).on('data', logfile));
}

function prepDocs(cb) {
    console.log('TODO'); cb();
}

var cleanBuild = cb => del(build, cb);
var cleanMove = cb => del(dstdir, { force: true }, cb);
var cleanAll = parallel(cleanBuild, cleanMove);

function help(cb) {
    console.log('Available tasks: ', tree().nodes);
    console.log('Use `gulp --tasks` to see more');
    cb();
}

module.exports = {
    sass: sassSource,
    inline: prepSource,
    move: series(cleanMove, moveSource),
    sitemap: sitemap,
    clean: cleanAll,
    build: series(cleanAll, sassSource, prepSource, moveSource, sitemap),
    default: help,
};
