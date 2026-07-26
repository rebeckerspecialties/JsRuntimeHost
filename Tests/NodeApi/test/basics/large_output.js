const output = 'O'.repeat(128 * 1024);
const error = 'E'.repeat(128 * 1024);

console.log(output);
console.error(error);
console.log('stdout-end');
console.error('stderr-end');
