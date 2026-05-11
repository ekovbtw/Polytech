import asyncio
import logging
import database as db
from handlers import build_application

logging.basicConfig(
    format="%(asctime)s %(name)s %(levelname)s %(message)s",
    level=logging.INFO,
)


def main():
    # Python 3.14 requires an explicit event loop before PTB can run
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    db.init_db()
    app = build_application()
    logging.info("Bot is running…")
    app.run_polling(drop_pending_updates=True)


if __name__ == "__main__":
    main()
