import { createRoot } from 'react-dom/client';
import { App } from './App';
import './styles.css';

const el = document.getElementById('root');
if (!el) throw new Error('#root hilang');
document.getElementById('boot')?.remove();
createRoot(el).render(<App />);
